"""Verify generated assets, three test maps and plugin portability."""
import unreal as u
from pathlib import Path
import json
PLUGIN="/HorrorSystems"
eal=u.EditorAssetLibrary
registry=u.AssetRegistryHelpers.get_asset_registry()
registry.scan_paths_synchronous([PLUGIN],force_rescan=True)
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
manny=eal.load_asset(PLUGIN+"/Characters/Mannequins/Meshes/SKM_Manny_Simple")
assert manny,"Missing mannequin"
paths=eal.list_assets(PLUGIN,True,False)
for path in paths:
    asset=eal.load_asset(path)
eal.save_directory(PLUGIN,only_if_is_dirty=True,recursive=True)
registry.scan_paths_synchronous([PLUGIN],force_rescan=True)
options=u.AssetRegistryDependencyOptions(include_soft_package_references=True,include_hard_package_references=True)
external=[]
for path in paths:
    package=path.split(".")[0]
    for dep in registry.get_dependencies(package,options):
        if str(dep).startswith("/Game/"): external.append({"asset":package,"dependency":str(dep)})
maps=[]
for letter in "ABC":
    path=PLUGIN+"/Maps/Test_"+letter
    assert levels.load_level(path),path
    all_actors=actors.get_all_level_actors()
    pickups=[a for a in all_actors if isinstance(a,u.HSPickup)]
    portals=[a for a in all_actors if isinstance(a,u.HSPortal)]
    monsters=[a for a in all_actors if isinstance(a,u.HSMonster)]
    bounds=[a for a in all_actors if isinstance(a,u.NavMeshBoundsVolume)]
    assert len(pickups)==(4 if letter=="A" else 3) and len(portals)==1 and len(monsters)==1 and len(bounds)==1,path+" actors missing"
    assert len({str(a.pickup_id) for a in pickups})==len(pickups),"Duplicate PickupId"
    origin,extent=bounds[0].get_actor_bounds(False)
    assert extent.x>=2500 and extent.y>=1700,"Navigation bounds too small: "+str(extent)
    directors=[a for a in all_actors if isinstance(a,u.HSRoomDirector)]
    windows=[a for a in all_actors if isinstance(a,u.HSWindowSequence)]
    props=[a for a in all_actors if isinstance(a,u.HSMovableProp)]
    assert len(directors)==len(windows)==len(props)==1,"Room systems missing"
    assert directors[0].rules and directors[0].window_sequence,"Room configuration missing"
    assert len(directors[0].rules.routes)==3 and directors[0].rules.duration==60,"Stage routes or timer missing"
    for p in portals: assert p.get_editor_property("destination") and p.use_stage_route and p.room_rules,"Portal route missing"
    maps.append({"map":path,"actors":len(all_actors),"pickups":len(pickups),"nav_extent":[extent.x,extent.y,extent.z]})
report={"plugin_assets":len(paths),"host_game_dependencies":external,"maps":maps,"status":"passed" if not external else "failed"}
target=Path(u.Paths.project_saved_dir())/"Verification/ContentAudit.json"
target.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
assert not external,"Plugin still depends on host /Game content: "+str(external)
u.log("HS_CONTENT_AUDIT_SUCCESS "+str(target))
