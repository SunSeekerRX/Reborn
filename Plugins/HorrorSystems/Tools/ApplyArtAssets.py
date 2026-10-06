"""Replace art on existing components without replacing their gameplay Actors."""
import unreal as u
from pathlib import Path
import json,hashlib,re,shutil

root='/HorrorSystems';eal=u.EditorAssetLibrary;assets=u.AssetToolsHelpers.get_asset_tools()
levels=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
meshes=u.get_editor_subsystem(u.StaticMeshEditorSubsystem);mel=u.MaterialEditingLibrary
u.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([root],force_rescan=True)
project=Path(u.Paths.project_dir());saved=Path(u.Paths.project_saved_dir(),'Verification')
backup=saved/'BasicBeforeArt';backup.mkdir(exist_ok=True)
for name in ('Basic_roomA','Basic_roomB','Basic_roomC','Basic_roomABC'):
    f=project/'Plugins/HorrorSystems/Content/Maps'/(name+'.umap')
    if not (backup/f.name).exists():shutil.copy2(f,backup/f.name)

def material(group,index,texture_index):
    name='M_'+group+'_'+str(index);path=root+'/Art/'+group+'/'+name
    mat=eal.load_asset(path) if eal.does_asset_exist(path) else assets.create_asset(name,root+'/Art/'+group,u.Material,u.MaterialFactoryNew())
    mel.delete_all_material_expressions(mat)
    sample=mel.create_material_expression(mat,u.MaterialExpressionTextureSample,0,0)
    sample.texture=eal.load_asset(root+'/Art/'+group+'/T_'+group+'_'+str(texture_index))
    assert sample.texture
    mel.connect_material_property(sample,'RGB',u.MaterialProperty.MP_BASE_COLOR)
    rough=mel.create_material_expression(mat,u.MaterialExpressionConstant,0,150);rough.r=.7
    mel.connect_material_property(rough,'',u.MaterialProperty.MP_ROUGHNESS)
    mat.set_editor_property('two_sided',True)
    if group=='Butcher':mat.set_editor_property('used_with_skeletal_mesh',True)
    mel.recompile_material(mat);assert eal.save_loaded_asset(mat)
    return mat

sources={}
for group in ('Bookshelf','Drawer','Sofa','Locker','Butcher'):
    mesh=eal.load_asset(root+'/Art/'+group+'/SM_'+group);assert mesh
    for i in range(len(mesh.static_materials)):
        texture_index=([0,1] if group=='Locker' else [0,1,3,2,2] if group=='Butcher' else [0]*20)[i]
        mesh.set_material(i,material(group,i,texture_index))
    assert eal.save_loaded_asset(mesh);sources[group]=mesh

# Rig uses the project's existing skeleton; locomotion blend space stays compatible.
path=root+'/Art/Butcher/SKM_Butcher'
rig=eal.load_asset(path) if eal.does_asset_exist(path) else None
if not rig or eal.get_metadata_tag(rig,'ButcherRigVersion')!='3':
    u.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
    task=u.AssetImportTask();task.filename=str(project/'ArtSource/Butcher/SKM_Butcher.fbx')
    task.destination_path=root+'/Art/Butcher';task.destination_name='SKM_Butcher';task.automated=True;task.replace_existing=True;task.save=True
    options=u.FbxImportUI();options.import_mesh=True;options.import_as_skeletal=True
    options.mesh_type_to_import=u.FBXImportType.FBXIT_SKELETAL_MESH;options.automated_import_should_detect_type=False
    options.import_animations=False;options.import_materials=False;options.import_textures=False
    options.skeleton=eal.load_asset(root+'/Characters/Mannequins/Meshes/SKM_Manny_Simple').skeleton
    options.create_physics_asset=False
    task.options=options;assets.import_asset_tasks([task]);rig=eal.load_asset(path)
assert isinstance(rig,u.SkeletalMesh),'Butcher skeletal import failed'
skeletal_materials=rig.get_editor_property('materials')
slot_map={'tripo_mat_cf8c122e':0,'tripo_mat_736c3145':1,'材质':2,'tripo_mat_b1265a5f':3,'Material_0':4}
for slot_index,slot in enumerate(skeletal_materials):
    name=str(slot.material_slot_name)
    index=next((v for token,v in slot_map.items() if token in name),4)
    slot.set_editor_property('material_interface',eal.load_asset(root+'/Art/Butcher/M_Butcher_'+str(index)))
    skeletal_materials[slot_index]=slot
rig.set_editor_property('materials',skeletal_materials);eal.set_metadata_tag(rig,'ButcherRigVersion','3');assert eal.save_loaded_asset(rig)

def match(label):
    s=label.lower()
    if any(t in s for t in ('iron cabinet','iron_cabinet','locker','铁柜')):return 'Locker'
    if any(t in s for t in ('bookshelf','book shelf','bookcase','书架','书柜')):return 'Bookshelf'
    if any(t in s for t in ('sofa','couch','沙发')):return 'Sofa'
    if any(t in s for t in ('drawer','cabinet','抽屉')):return 'Drawer'
    return None

def fitted_mesh(actor,component,group):
    old=component.static_mesh
    if eal.get_metadata_tag(old,'RebornFurnitureFacing')=='3':return old
    if old.get_path_name().startswith(root+'/Art/Fitted/'):return old
    source=sources[group]
    center,extent=actor.get_actor_bounds(False)
    transform=actor.get_actor_transform()
    bounds=source.get_bounds();origin=bounds.origin;source_extent=bounds.box_extent
    yaw=0
    if (source_extent.x>source_extent.y)!=(extent.x>extent.y):yaw=90
    elif source_extent.y>source_extent.x:yaw=0
    else:yaw=0
    # Rotate generic Blender fronts toward the room rather than into their supporting wall.
    if center.x>-550 and extent.y>extent.x:yaw+=180
    key=hashlib.sha1((group+old.get_path_name()+str(actor.get_actor_rotation())+str(actor.get_actor_scale3d())+str(extent)+str(yaw)).encode()).hexdigest()[:12]
    path=root+'/Art/Fitted/SM_'+group+'_'+key
    target=eal.load_asset(path) if eal.does_asset_exist(path) else None
    if target and eal.get_metadata_tag(target,'RebornFitted')=='1':return target
    if not target:target=assets.duplicate_asset('SM_'+group+'_'+key,root+'/Art/Fitted',source)
    dm=u.DynamicMesh()
    _,outcome=u.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(source,dm,u.GeometryScriptCopyMeshFromAssetOptions(),u.GeometryScriptMeshReadLOD())
    assert outcome==u.GeometryScriptOutcomePins.SUCCESS
    u.GeometryScript_MeshTransforms.transform_mesh(dm,u.Transform(location=-origin))
    u.GeometryScript_MeshTransforms.transform_mesh(dm,u.Transform(rotation=u.Rotator(yaw=yaw)))
    rotated=u.Vector(source_extent.y,source_extent.x,source_extent.z) if yaw%180 else source_extent
    scale=u.Vector(extent.x/max(.001,rotated.x),extent.y/max(.001,rotated.y),extent.z/max(.001,rotated.z))
    u.GeometryScript_MeshTransforms.transform_mesh(dm,u.Transform(scale=scale))
    u.GeometryScript_MeshTransforms.transform_mesh(dm,u.Transform(location=center))
    u.GeometryScript_MeshTransforms.transform_mesh(dm,u.MathLibrary.invert_transform(transform))
    _,outcome=u.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(dm,target,u.GeometryScriptCopyMeshToAssetOptions(),u.GeometryScriptMeshWriteLOD())
    assert outcome==u.GeometryScriptOutcomePins.SUCCESS
    target.get_editor_property('body_setup').set_editor_property('collision_trace_flag',u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    eal.set_metadata_tag(target,'RebornFitted','1')
    assert eal.save_loaded_asset(target)
    return target

report=[]
for name in ('Basic_roomA','Basic_roomB','Basic_roomC','Basic_roomABC'):
    assert levels.load_level(root+'/Maps/'+name)
    rows=[]
    for actor in list(actors.get_all_level_actors()):
        if isinstance(actor,(u.HSInspectTrigger,u.HSPickup,u.HSPortal)):continue
        group=match(actor.get_actor_label())
        component=actor.get_component_by_class(u.StaticMeshComponent) if group else None
        if not component or not component.static_mesh:continue
        original_transform=actor.get_actor_transform();original_components=[c.get_name() for c in actor.get_components_by_class(u.ActorComponent)]
        fitted=fitted_mesh(actor,component,group);component.set_static_mesh(fitted)
        for i,m in enumerate(fitted.static_materials):component.set_material(i,m.material_interface)
        assert actor.get_actor_transform()==original_transform,'Actor transform changed'
        assert [c.get_name() for c in actor.get_components_by_class(u.ActorComponent)]==original_components,'Gameplay components changed'
        rows.append({'label':actor.get_actor_label(),'type':actor.get_class().get_name(),'group':group,'mesh':fitted.get_path_name(),'components_preserved':original_components})
    audio=[a for a in actors.get_all_level_actors() if isinstance(a,u.HSSceneAudio)]
    if not audio:
        a=actors.spawn_actor_from_class(u.HSSceneAudio,u.Vector(0,0,0));a.set_actor_label('Reborn_SceneAudio')
    for actor in list(actors.get_all_level_actors()):
        if not actor.get_actor_label().startswith('Reborn_WindowLight_'):continue
        actor.point_light_component.set_mobility(u.ComponentMobility.MOVABLE)
        actor.point_light_component.set_editor_property('intensity_units',u.LightUnits.LUMENS)
        actor.point_light_component.set_editor_property('intensity',350.0)
        actor.point_light_component.set_editor_property('attenuation_radius',600.0)
        label=actor.get_actor_label().replace('WindowLight','WindowFill')
        fill=next((a for a in actors.get_all_level_actors() if a.get_actor_label()==label),None)
        if not fill:
            fill=actors.spawn_actor_from_class(u.PointLight,actor.get_actor_location()+u.Vector(-75,-100,-80));fill.set_actor_label(label)
        fill.point_light_component.set_mobility(u.ComponentMobility.MOVABLE)
        fill.point_light_component.set_editor_property('intensity_units',u.LightUnits.LUMENS)
        fill.point_light_component.set_editor_property('intensity',180.0)
        fill.point_light_component.set_editor_property('attenuation_radius',450.0)
    assert levels.save_current_level()
    report.append({'map':name,'replacements':rows})
(saved/'ArtReplacementReport.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
u.log('REBORN_ART_INSTALLED')
# The shared repair pass calibrates each source's true front, preserves mirrored
# Actor transforms, and restores authored upright locker proportions.
import runpy
runpy.run_path(str(Path(__file__).with_name('RepairInteractionArt.py')))
