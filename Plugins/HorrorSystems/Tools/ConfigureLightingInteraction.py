"""Dim authored lighting and seat collectible meshes on real tabletops without replacing Actors."""
import unreal as u,json,shutil
from pathlib import Path
p=Path(u.Paths.project_dir());v=Path(u.Paths.project_saved_dir(),'Verification')
u.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(['/HorrorSystems'],force_rescan=True)
levels=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
backup=v/'BeforeLightingInteraction';backup.mkdir(parents=True,exist_ok=True)
for f in (p/'Plugins/HorrorSystems/Content/Maps').glob('Basic_room*.umap'):
 if not (backup/f.name).exists():shutil.copy2(f,backup/f.name)
def bounds(comp):
 b=comp.static_mesh.get_bounds();t=comp.get_world_transform();corners=[t.transform_location(b.origin+u.Vector(x*b.box_extent.x,y*b.box_extent.y,z*b.box_extent.z)) for x in (-1,1) for y in (-1,1) for z in (-1,1)]
 lo=u.Vector(*[min(getattr(c,k) for c in corners) for k in ('x','y','z')]);hi=u.Vector(*[max(getattr(c,k) for c in corners) for k in ('x','y','z')]);return (lo+hi)*.5,(hi-lo)*.5
report=[]
for name in ('Basic_roomA','Basic_roomB','Basic_roomC','Basic_roomABC'):
 assert levels.load_level('/HorrorSystems/Maps/'+name)
 allactors=actors.get_all_level_actors();row={'map':name,'lights':[],'items':[]};tables=[a for a in allactors if u.Name('Reborn_NewDiningTable') in a.tags];assert tables
 for actor in allactors:
  for c in actor.get_components_by_class(u.LightComponentBase):
   if isinstance(actor,u.HSMonster) or isinstance(actor,u.HSPortal):continue
   before=c.get_editor_property('intensity');label=actor.get_actor_label()
   if isinstance(c,u.DirectionalLightComponent):after=min(before,.08)
   elif isinstance(c,u.SkyLightComponent):after=min(before,.03)
   elif isinstance(c,u.PointLightComponent):
    after=75 if 'WindowLight' in label else 35 if 'WindowFill' in label else min(before,60)
    if 'Window' in label:c.set_light_color(u.LinearColor(1,.025,.01,1))
   else:continue
   c.set_editor_property('intensity',after);row['lights'].append({'actor':label,'before':before,'after':after})
 cache={}
 for actor in allactors:
  if not isinstance(actor,(u.HSPickup,u.HSInspectTrigger)):continue
  item=actor.get_editor_property('item_data');assert item
  comp=actor.get_component_by_class(u.StaticMeshComponent);assert comp.static_mesh
  original=actor.get_actor_location();table=min(tables,key=lambda a:(a.get_actor_bounds(False)[0]-original).length())
  tc,te=table.get_actor_bounds(False);components=[c.get_name() for c in actor.get_components_by_class(u.ActorComponent)]
  c,e=bounds(comp);maximum=15 if isinstance(actor,u.HSPickup) and str(actor.clue_id)!='None' else 18
  factor=min(1,maximum/max(e.x,e.y));comp.set_relative_scale3d(comp.get_editor_property('relative_scale3d')*factor)
  c,e=bounds(comp)
  key=isinstance(actor,u.HSPickup) and str(actor.clue_id)!='None'
  ox,oy=(.45,.3) if key else (.45,-.3) if isinstance(actor,u.HSInspectTrigger) else (-.05,.3)
  x,y=tc.x+ox*te.x,tc.y+oy*te.y
  if table.get_name() not in cache:
   dm=u.DynamicMesh();tcomp=table.get_component_by_class(u.StaticMeshComponent)
   _,out=u.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(tcomp.static_mesh,dm,u.GeometryScriptCopyMeshFromAssetOptions(),u.GeometryScriptMeshReadLOD());assert out==u.GeometryScriptOutcomePins.SUCCESS
   u.GeometryScript_MeshTransforms.transform_mesh(dm,tcomp.get_world_transform());_,bvh=u.GeometryScript_MeshSpatial.build_bvh_for_mesh(dm);cache[table.get_name()]=(dm,bvh)
  dm,bvh=cache[table.get_name()]
  _,hit,out=u.GeometryScript_MeshSpatial.find_nearest_ray_intersection_with_mesh(dm,bvh,u.Vector(x,y,tc.z+te.z+100),u.Vector(0,0,-1),u.GeometryScriptSpatialQueryOptions());assert hit.hit,(name,actor.get_actor_label())
  actor.set_actor_location(original+u.Vector(x-c.x,y-c.y,hit.hit_position.z+.3-(c.z-e.z)),False,False)
  if isinstance(actor,u.HSPickup):actor.rotate_for_demo=False
  assert components==[c.get_name() for c in actor.get_components_by_class(u.ActorComponent)]
  nc,ne=bounds(comp);assert abs(nc.z-ne.z-hit.hit_position.z-.3)<.02
  row['items'].append({'actor':actor.get_actor_label(),'table':table.get_actor_label(),'surface_z':hit.hit_position.z,'bottom_z':nc.z-ne.z,'location':str(actor.get_actor_location()),'mesh_extent':str(ne),'components_preserved':components})
 assert levels.save_current_level();report.append(row)
(v/'LightingInteractionPlacement.json').write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf-8')
u.log('LIGHTING_INTERACTION_APPLIED')

