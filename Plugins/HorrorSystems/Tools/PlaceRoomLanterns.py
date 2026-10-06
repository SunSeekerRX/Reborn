import unreal as u
from pathlib import Path
import json
Path(u.Paths.project_saved_dir(),'Verification').mkdir(parents=True,exist_ok=True)
u.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(['/HorrorSystems'],force_rescan=True)
levels=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem);report=[]
for name in ('Basic_roomA','Basic_roomB','Basic_roomC','Basic_roomABC'):
    assert levels.load_level('/HorrorSystems/Maps/'+name)
    allactors=actors.get_all_level_actors()
    for lamp in allactors:
        if not lamp.get_actor_label().startswith('Reborn_KeroseneLamp_'):continue
        c,e=lamp.get_actor_bounds(False)
        tables=[a for a in allactors if u.Name('Reborn_NewDiningTable') in a.tags]
        table=min(tables,key=lambda a:(a.get_actor_bounds(False)[0]-c).length());tc,te=table.get_actor_bounds(False)
        x,y=tc.x-te.x*.55,tc.y-te.y*.55
        comp=table.get_component_by_class(u.StaticMeshComponent);dm=u.DynamicMesh()
        _,out=u.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(comp.static_mesh,dm,u.GeometryScriptCopyMeshFromAssetOptions(),u.GeometryScriptMeshReadLOD());assert out==u.GeometryScriptOutcomePins.SUCCESS
        u.GeometryScript_MeshTransforms.transform_mesh(dm,comp.get_world_transform())
        _,bvh=u.GeometryScript_MeshSpatial.build_bvh_for_mesh(dm)
        _,hit,out=u.GeometryScript_MeshSpatial.find_nearest_ray_intersection_with_mesh(dm,bvh,u.Vector(x,y,tc.z+te.z+100),u.Vector(0,0,-1),u.GeometryScriptSpatialQueryOptions())
        assert hit.hit,(name,table.get_actor_label(),hit)
        location=hit.hit_position
        lamp.set_actor_location(lamp.get_actor_location()+u.Vector(x-c.x,y-c.y,location.z+.2-(c.z-e.z)),False,False)
        report.append({'map':name,'lamp':lamp.get_actor_label(),'table':table.get_actor_label(),'surface':str(location),'bottom':lamp.get_actor_bounds(False)[0].z-lamp.get_actor_bounds(False)[1].z})
    assert levels.save_current_level()
Path(u.Paths.project_saved_dir(),'Verification/LanternPlacement.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
u.log('LANTERN_PLACEMENT_VERIFIED')
