"""Repair only existing Basic room shells; retain furniture and all Actor settings."""
import unreal as u
from pathlib import Path
import sys,json,shutil
sys.path.insert(0,str(Path(__file__).resolve().parent))
from BasicGeometry import cut_basic_room_openings

eal=u.EditorAssetLibrary
u.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(['/HorrorSystems'],force_rescan=True)
levels=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
saved=Path(u.Paths.project_saved_dir(),'Verification');backup=saved/'BeforeSafeExitFloorRepair';backup.mkdir(exist_ok=True)
report=[]
for name in ('Basic_roomA','Basic_roomB','Basic_roomC','Basic_roomABC'):
    assert levels.load_level('/HorrorSystems/Maps/'+name)
    shells=[a for a in actors.get_all_level_actors() if isinstance(a,u.StaticMeshActor) and a.get_actor_label().startswith('CubeGrid')]
    for director in [a for a in actors.get_all_level_actors() if isinstance(a,u.HSRoomDirector)]:
        window_y=director.window_sequence.get_actor_location().y
        safe=director.get_actor_location();nominal_floor_z=safe.z-200
        shell=min(shells,key=lambda a:abs(a.get_actor_bounds(False)[0].y-window_y))
        target=shell.static_mesh_component.static_mesh
        original=eal.get_metadata_tag(target,'OriginalShellMesh');assert original,target.get_path_name()
        source=eal.load_asset(original);assert source,original
        file=Path(u.Paths.project_dir(),'Plugins/HorrorSystems/Content/Rooms',target.get_name()+'.uasset')
        if not (backup/file.name).exists():shutil.copy2(file,backup/file.name)
        row=cut_basic_room_openings(source,target,shell.get_actor_transform(),safe.y+(300 if window_y>safe.y else -300),window_y,nominal_floor_z)
        row['map']=name;row['room']=str(director.rules.room_id);report.append(row)
        u.log('SAFE_EXIT_FIXED '+str(row))
(saved/'SafeExitFloorRepair.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
u.log('SAFE_EXIT_FLOORS_REPAIRED')
