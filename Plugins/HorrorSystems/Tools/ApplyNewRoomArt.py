"""Apply new wall/furniture art while retaining authored collision and gameplay Actors."""
import unreal as u
from pathlib import Path
import json,hashlib,shutil
root='/HorrorSystems';project=Path(u.Paths.project_dir());saved=Path(u.Paths.project_saved_dir(),'Verification')
u.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([root],force_rescan=True)
eal=u.EditorAssetLibrary;assets=u.AssetToolsHelpers.get_asset_tools();mel=u.MaterialEditingLibrary
levels=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
backup=saved/'BeforeNewRoomArt';backup.mkdir(parents=True,exist_ok=True)
for f in (project/'Plugins/HorrorSystems/Content/Maps').glob('Basic_room*.umap'):
    if not (backup/f.name).exists():shutil.copy2(f,backup/f.name)

def vec(v):return [v.x,v.y,v.z]
def node(mat,cls):return mel.create_material_expression(mat,cls,0,0)
def fitted(actor,group,center,extent,yaw):
    component=actor.get_component_by_class(u.StaticMeshComponent);source=eal.load_asset(root+'/Art/'+group+'/SM_'+group);assert source
    transform=actor.get_actor_transform();components=[c.get_name() for c in actor.get_components_by_class(u.ActorComponent)]
    key=hashlib.sha1((group+str(center)+str(extent)+str(component.get_world_transform())+str(yaw)).encode()).hexdigest()[:12]
    path=root+'/Art/Fitted/SM_'+group+'_NewRoom_'+key
    target=eal.load_asset(path) if eal.does_asset_exist(path) else None
    if not target:
        target=assets.duplicate_asset('SM_'+group+'_NewRoom_'+key,root+'/Art/Fitted',source)
        dm=u.DynamicMesh();_,out=u.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(source,dm,u.GeometryScriptCopyMeshFromAssetOptions(),u.GeometryScriptMeshReadLOD());assert out==u.GeometryScriptOutcomePins.SUCCESS
        b=source.get_bounds();se=b.box_extent
        u.GeometryScript_MeshTransforms.transform_mesh(dm,u.Transform(location=-b.origin))
        u.GeometryScript_MeshTransforms.transform_mesh(dm,u.Transform(rotation=u.Rotator(yaw=yaw)))
        re=u.Vector(se.y,se.x,se.z) if abs(yaw)%180 else se
        u.GeometryScript_MeshTransforms.transform_mesh(dm,u.Transform(scale=u.Vector(extent.x/re.x,extent.y/re.y,extent.z/re.z)))
        u.GeometryScript_MeshTransforms.transform_mesh(dm,u.Transform(location=center))
        t=component.get_world_transform();q=t.rotation;s=t.scale3d
        u.GeometryScript_MeshTransforms.transform_mesh(dm,u.Transform(location=-t.translation))
        u.GeometryScript_MeshTransforms.transform_mesh(dm,u.Transform(rotation=u.Quat(x=-q.x,y=-q.y,z=-q.z,w=q.w).rotator()))
        u.GeometryScript_MeshTransforms.transform_mesh(dm,u.Transform(scale=u.Vector(1/s.x,1/s.y,1/s.z)))
        _,out=u.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(dm,target,u.GeometryScriptCopyMeshToAssetOptions(),u.GeometryScriptMeshWriteLOD());assert out==u.GeometryScriptOutcomePins.SUCCESS
        target.get_editor_property('body_setup').set_editor_property('collision_trace_flag',u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        eal.set_metadata_tag(target,'NewRoomArtGroup',group);assert eal.save_loaded_asset(target)
    component.set_static_mesh(target)
    for i,slot in enumerate(source.static_materials):component.set_material(i,slot.material_interface)
    assert actor.get_actor_transform()==transform
    assert components==[c.get_name() for c in actor.get_components_by_class(u.ActorComponent)]
    c,e=actor.get_actor_bounds(False);assert (c-center).length()<2 and (e-extent).length()<2,(actor.get_actor_label(),c,e,center,extent)
    return {'label':actor.get_actor_label(),'group':group,'center':vec(c),'extent':vec(e),'mesh':target.get_path_name(),'components_preserved':components}

def wall_material(original,map_name):
    while isinstance(original,u.MaterialInstance):original=original.get_editor_property('parent')
    assert isinstance(original,u.Material),original
    name='M_WallsV1_'+map_name+'_'+hashlib.sha1(original.get_path_name().encode()).hexdigest()[:8]
    path=root+'/Art/Walls/'+name
    if eal.does_asset_exist(path):return eal.load_asset(path)
    mat=assets.duplicate_asset(name,root+'/Art/Walls',original)
    old=mel.get_material_property_input_node(mat,u.MaterialProperty.MP_BASE_COLOR)
    old_output=mel.get_material_property_input_node_output_name(mat,u.MaterialProperty.MP_BASE_COLOR)
    if not old:
        old=node(mat,u.MaterialExpressionConstant3Vector);old.constant=u.LinearColor(.5,.5,.5,1);old_output=''
    custom=node(mat,u.MaterialExpressionCustom)
    custom.set_editor_property('output_type',u.CustomMaterialOutputType.CMOT_FLOAT3)
    names=['WorldPos','Normal','RoomTexture','SafeTexture','OriginalColor']
    inputs=[]
    for n in names:
        inp=u.CustomInput();inp.set_editor_property('input_name',n);inputs.append(inp)
    custom.set_editor_property('inputs',inputs)
    boxes=[(-200,-900,300,300)] if map_name!='Basic_roomABC' else [(-200,-900,300,300),(-200,-1500,300,300),(-199,-5497,300,300),(-200,-8698,300,300)]
    code='float3 n=abs(Normal); float2 uv=float2(n.x>n.y?WorldPos.y:WorldPos.x,-WorldPos.z)/200.0; float safe=0;\n'
    for x,y,ex,ey in boxes:code+=f'safe=max(safe,step(abs(WorldPos.x-({x})),{ex+5})*step(abs(WorldPos.y-({y})),{ey+5}));\n'
    code+='float3 room=Texture2DSample(RoomTexture,RoomTextureSampler,uv).rgb; float3 refuge=Texture2DSample(SafeTexture,SafeTextureSampler,uv).rgb; return lerp(OriginalColor,lerp(room,refuge,safe),step(n.z,0.5));'
    custom.set_editor_property('code',code)
    pos=node(mat,u.MaterialExpressionWorldPosition);normal=node(mat,u.MaterialExpressionVertexNormalWS)
    room=node(mat,u.MaterialExpressionTextureObject);room.texture=eal.load_asset(root+'/Art/RoomWall/T_RoomWall')
    safe=node(mat,u.MaterialExpressionTextureObject);safe.texture=eal.load_asset(root+'/Art/SafeWall/T_SafeWall')
    for source,out,dest in [(pos,'','WorldPos'),(normal,'','Normal'),(room,'','RoomTexture'),(safe,'','SafeTexture'),(old,old_output,'OriginalColor')]:
        assert mel.connect_material_expressions(source,out,custom,dest)
    assert mel.connect_material_property(custom,'',u.MaterialProperty.MP_BASE_COLOR)
    eal.set_metadata_tag(mat,'OriginalFloorMaterial',original.get_path_name())
    mel.recompile_material(mat);assert eal.save_loaded_asset(mat);return mat

# Imported brass and glass slots are distinct; retain transparent lamp glass.
glass_path=root+'/Art/Lantern/M_LanternGlass'
glass=eal.load_asset(glass_path) if eal.does_asset_exist(glass_path) else assets.create_asset('M_LanternGlass',root+'/Art/Lantern',u.Material,u.MaterialFactoryNew())
mel.delete_all_material_expressions(glass);glass.set_editor_property('blend_mode',u.BlendMode.BLEND_TRANSLUCENT);glass.set_editor_property('two_sided',True)
color=node(glass,u.MaterialExpressionConstant3Vector);color.constant=u.LinearColor(.6,.7,.75,1)
opacity=node(glass,u.MaterialExpressionConstant);opacity.r=.12
mel.connect_material_property(color,'',u.MaterialProperty.MP_BASE_COLOR);mel.connect_material_property(opacity,'',u.MaterialProperty.MP_OPACITY)
mel.recompile_material(glass);assert eal.save_loaded_asset(glass)
lamp_source=eal.load_asset(root+'/Art/Lantern/SM_Lantern')
for i,slot in enumerate(lamp_source.static_materials):
    if 'glass' in str(slot.material_slot_name).lower():lamp_source.set_material(i,glass)
assert eal.save_loaded_asset(lamp_source)

report=[]
for name in ('Basic_roomA','Basic_roomB','Basic_roomC','Basic_roomABC'):
    assert levels.load_level(root+'/Maps/'+name)
    all_actors=list(actors.get_all_level_actors());row={'map':name,'walls':[],'tables':[],'bookshelves':[],'lamps':[],'hidden_table_legs':[]}
    for actor in all_actors:
        label=actor.get_actor_label();component=actor.get_component_by_class(u.StaticMeshComponent)
        if not component or not component.static_mesh:continue
        if label.startswith('CubeGrid'):
            mesh=component.static_mesh;before=actor.get_actor_transform()
            for i in range(len(mesh.static_materials)):
                original=component.get_material(i) or eal.load_asset('/Engine/EngineMaterials/WorldGridMaterial')
                if original and not original.get_path_name().startswith(root+'/Art/Walls/'):
                    mat=wall_material(original,name);component.set_material(i,mat)
                    row['walls'].append({'actor':label,'slot':i,'original':original.get_path_name(),'new':mat.get_path_name()})
            assert before==actor.get_actor_transform()
    tables=[]
    for actor in all_actors:
        if not isinstance(actor,u.StaticMeshActor) or actor.get_editor_property('is_editor_only_actor'):continue
        label=actor.get_actor_label();c,e=actor.get_actor_bounds(False)
        if label.startswith('Box') and e.x>100 and e.y>150 and e.z<15 and 180<c.z<225:
            legs=[a for a in all_actors if isinstance(a,u.StaticMeshActor) and a!=actor and a.get_actor_label().startswith('Box') and (a.get_actor_bounds(False)[0]-c).length()<300 and a.get_actor_bounds(False)[1].x<15 and a.get_actor_bounds(False)[1].y<15]
            row['tables'].append(fitted(actor,'DiningTable',u.Vector(c.x,c.y,(c.z+e.z+100)/2),u.Vector(e.x,e.y,(c.z+e.z-100)/2),0 if e.y>e.x else 90))
            actor.tags=list(actor.tags)+[u.Name('Reborn_NewDiningTable')];tables.append(actor)
            for leg in legs:
                leg.set_actor_hidden_in_game(True);leg.set_actor_enable_collision(False);leg.set_editor_property('is_editor_only_actor',True);row['hidden_table_legs'].append(leg.get_actor_label())
        elif 'table' in label.lower() or '桌' in label:
            row['tables'].append(fitted(actor,'DiningTable',u.Vector(c.x,c.y,(c.z+e.z+100)/2),u.Vector(e.x,e.y,(c.z+e.z-100)/2),0 if e.y>=e.x else 90))
            actor.tags=list(actor.tags)+[u.Name('Reborn_NewDiningTable')];tables.append(actor)
    for actor in all_actors:
        if (name=='Basic_roomB' and isinstance(actor,u.HSCollapsingObstacle)) or (name=='Basic_roomABC' and actor.get_actor_label()=='Bookshelf2'):
            c,e=actor.get_actor_bounds(False)
            row['bookshelves'].append(fitted(actor,'BookshelfNew',c,e,0 if c.x<-550 else 180))
    # One lamp per independent room or per A/B/C section in the combined level.
    room_centers=[750] if name!='Basic_roomABC' else [750,-2800,-7000]
    for section,room_y in enumerate(room_centers):
        choices=[a for a in tables if abs(a.get_actor_bounds(False)[0].y-room_y)<1700]
        if not choices:continue
        table=min(choices,key=lambda a:abs(a.get_actor_bounds(False)[0].y-room_y))
        c,e=table.get_actor_bounds(False);label='Reborn_KeroseneLamp_'+str(section+1)
        lamp=next((a for a in all_actors if a.get_actor_label()==label),None)
        if not lamp:lamp=actors.spawn_actor_from_class(u.StaticMeshActor,u.Vector(c.x,c.y,c.z+e.z))
        lamp.set_actor_label(label);lamp.static_mesh_component.set_static_mesh(lamp_source)
        row['lamps'].append(fitted(lamp,'Lantern',u.Vector(c.x,c.y,c.z+e.z+25),u.Vector(10.37,8.25,25),0))
        lamp.static_mesh_component.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
        lamp.tags=[u.Name('Reborn_NewKeroseneLamp')]
    assert levels.save_current_level();report.append(row)
    (saved/'NewRoomArtApplied.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
u.log('NEW_ROOM_ART_APPLIED')
