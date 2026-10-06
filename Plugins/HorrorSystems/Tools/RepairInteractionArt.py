"""Repair authored reference actors and furniture fronts without replacing Actors."""
import unreal as u
from pathlib import Path
import json, hashlib, shutil

root='/HorrorSystems'
u.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([root],force_rescan=True)
levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
eal=u.EditorAssetLibrary; assets=u.AssetToolsHelpers.get_asset_tools()
project=Path(u.Paths.project_dir());saved=Path(u.Paths.project_saved_dir(),'Verification')
backup=saved/'BeforeInteractionArtFix';backup.mkdir(exist_ok=True)
for f in (project/'Plugins/HorrorSystems/Content/Maps').glob('Basic_room*.umap'):
    if not (backup/f.name).exists():shutil.copy2(f,backup/f.name)

def match(label):
    s=label.lower()
    for group,tokens in [('Locker',('iron cabinet','iron_cabinet','locker','铁柜')),('Bookshelf',('bookshelf','bookcase','书架','书柜')),('Drawer',('drawer','cabinet','抽屉')),('Sofa',('sofa','couch','沙发'))]:
        if any(t in s for t in tokens):return group

def vec(v):return [v.x,v.y,v.z]

report=[]
for name in ('Basic_roomA','Basic_roomB','Basic_roomC','Basic_roomABC'):
    assert levels.load_level(root+'/Maps/'+name)
    row={'map':name,'editor_only_references':[],'furniture':[]}
    for actor in actors.get_all_level_actors():
        label=actor.get_actor_label()
        if isinstance(actor,u.SkeletalMeshActor) and ('mannequin' in label.lower() or 'manny' in label.lower()):
            actor.set_actor_hidden_in_game(True);actor.set_actor_enable_collision(False)
            actor.skeletal_mesh_component.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
            actor.set_editor_property('is_editor_only_actor',True)
            assert actor.get_editor_property('is_editor_only_actor')
            row['editor_only_references'].append(label);continue
        if isinstance(actor,(u.HSInspectTrigger,u.HSPickup,u.HSPortal)):continue
        group=match(label);component=actor.get_component_by_class(u.StaticMeshComponent) if group else None
        if not component or not component.static_mesh:continue
        source=eal.load_asset(root+'/Art/'+group+'/SM_'+group);assert source
        if eal.get_metadata_tag(component.static_mesh,'RebornFurnitureFacing')=='3' and group!='Drawer':
            c,e=actor.get_actor_bounds(False)
            row['furniture'].append({'label':label,'class':actor.get_class().get_name(),'group':group,'center':vec(c),'extent':vec(e),'mesh':component.static_mesh.get_path_name(),'already_fixed':True})
            continue
        center,extent=actor.get_actor_bounds(False)
        original_transform=actor.get_actor_transform()
        components=[c.get_name() for c in actor.get_components_by_class(u.ActorComponent)]
        # The prior fitter rolled the locker sideways through positional Rotator arguments.
        # Restore the dimensions of its authored white-box marker.
        if group=='Locker':extent=u.Vector(30,45,100)
        # Source fronts were verified by four rendered views: Bookshelf/Sofa +X,
        # Drawer/Locker +Y. Only yaw is changed; their original upright Z is retained.
        if group in ('Bookshelf','Locker','Sofa') or (group=='Drawer' and extent.y>extent.x):
            desired_yaw=0 if center.x < -550 else 180
        else:
            room_y=750 if name!='Basic_roomABC' or center.y>-1200 else (-2800 if center.y>-4500 else -7000)
            desired_yaw=90 if center.y < room_y else -90
        source_front=90 if group in ('Drawer','Locker') else 0
        yaw=desired_yaw-source_front
        key=hashlib.sha1((group+str(center)+str(extent)+str(component.get_world_transform())+str(yaw)).encode()).hexdigest()[:12]
        path=root+'/Art/Fitted/SM_'+group+'_FacingV3_'+key
        target=eal.load_asset(path) if eal.does_asset_exist(path) else None
        if not target or eal.get_metadata_tag(target,'RebornFurnitureFacing')!='3':
            if not target:target=assets.duplicate_asset('SM_'+group+'_FacingV3_'+key,root+'/Art/Fitted',source)
            dm=u.DynamicMesh()
            _,outcome=u.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(source,dm,u.GeometryScriptCopyMeshFromAssetOptions(),u.GeometryScriptMeshReadLOD())
            assert outcome==u.GeometryScriptOutcomePins.SUCCESS
            bounds=source.get_bounds();se=bounds.box_extent
            u.GeometryScript_MeshTransforms.transform_mesh(dm,u.Transform(location=-bounds.origin))
            u.GeometryScript_MeshTransforms.transform_mesh(dm,u.Transform(rotation=u.Rotator(yaw=yaw)))
            re=u.Vector(se.y,se.x,se.z) if abs(yaw)%180 else se
            u.GeometryScript_MeshTransforms.transform_mesh(dm,u.Transform(scale=u.Vector(extent.x/re.x,extent.y/re.y,extent.z/re.z)))
            u.GeometryScript_MeshTransforms.transform_mesh(dm,u.Transform(location=center))
            # Apply the inverse as separate operations. A single inverse FTransform
            # cannot express rotated nonuniform/negative scales from the authored boxes.
            t=component.get_world_transform();q=t.rotation;s=t.scale3d
            u.GeometryScript_MeshTransforms.transform_mesh(dm,u.Transform(location=-t.translation))
            u.GeometryScript_MeshTransforms.transform_mesh(dm,u.Transform(rotation=u.Quat(x=-q.x,y=-q.y,z=-q.z,w=q.w).rotator()))
            u.GeometryScript_MeshTransforms.transform_mesh(dm,u.Transform(scale=u.Vector(1/s.x,1/s.y,1/s.z)))
            _,outcome=u.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(dm,target,u.GeometryScriptCopyMeshToAssetOptions(),u.GeometryScriptMeshWriteLOD())
            assert outcome==u.GeometryScriptOutcomePins.SUCCESS
            target.get_editor_property('body_setup').set_editor_property('collision_trace_flag',u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
            eal.set_metadata_tag(target,'RebornFurnitureFacing','3');assert eal.save_loaded_asset(target)
        component.set_static_mesh(target)
        for i,m in enumerate(source.static_materials):component.set_material(i,m.material_interface)
        assert actor.get_actor_transform()==original_transform
        assert components==[c.get_name() for c in actor.get_components_by_class(u.ActorComponent)]
        new_center,new_extent=actor.get_actor_bounds(False)
        assert (new_center-center).length()<2,(label,center,new_center,original_transform,component.get_world_transform(),target.get_bounds())
        assert (new_extent-extent).length()<2,(label,extent,new_extent)
        row['furniture'].append({'label':label,'class':actor.get_class().get_name(),'group':group,'center':vec(center),'extent':vec(new_extent),'front_yaw':desired_yaw,'mesh':target.get_path_name(),'components_preserved':components})
    assert levels.save_current_level();report.append(row)
    (saved/'InteractionArtFix.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
u.log('INTERACTION_ART_FIXED')
