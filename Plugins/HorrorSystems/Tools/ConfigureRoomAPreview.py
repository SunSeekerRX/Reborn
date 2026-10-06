"""Configure only RoomA for the current playable preview; preserve other maps."""
import unreal as u
from pathlib import Path
import shutil,json
levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
saved=Path(u.Paths.project_saved_dir(),'Verification')
backup=saved/'BeforeRoomAPreview';backup.mkdir(parents=True,exist_ok=True)
source=Path(u.Paths.project_dir(),'Plugins/HorrorSystems/Content/Maps/Basic_roomA.umap')
if not (backup/source.name).exists():shutil.copy2(source,backup/source.name)
assert levels.load_level('/HorrorSystems/Maps/Basic_roomA')
windows=[a for a in actors.get_all_level_actors() if isinstance(a,u.HSWindowSequence)]
assert len(windows)==1
window=windows[0]
window.camera_waypoints=[u.Vector(-220,-250,300),u.Vector(-220,-100,300)]
window.camera_collision_radius=16
doors=[a for a in actors.get_all_level_actors() if isinstance(a,u.HSPortal)]
assert doors
for door in doors:door.travel_enabled=False
references=[]
for actor in actors.get_all_level_actors():
    if isinstance(actor,u.SkeletalMeshActor) and actor.get_actor_label()=='SK_Mannequin':
        actor.set_actor_hidden_in_game(True)
        actor.set_actor_enable_collision(False)
        actor.skeletal_mesh_component.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
        actor.set_editor_property('is_editor_only_actor',True)
        references.append(actor.get_actor_label())
assert levels.save_current_level()
(saved/'RoomAPreviewConfig.json').write_text(json.dumps({'map':'Basic_roomA','camera_waypoints':[[-220,-250,300],[-220,-100,300]],'camera_collision_radius':16,'disabled_room_exits':[a.get_actor_label() for a in doors],'editor_only_mannequins':references},indent=2),encoding='utf-8')
u.log('ROOM_A_PREVIEW_CONFIGURED')
