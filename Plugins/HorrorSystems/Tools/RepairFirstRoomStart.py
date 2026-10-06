"""Repair the first-room spawn and overlapping reference without deleting geometry."""
import unreal as u
from pathlib import Path
import sys, shutil, json
sys.path.insert(0, str(Path(__file__).resolve().parent))
from FirstRoomSetup import configure_first_room_references

project = Path(u.Paths.project_dir())
saved = Path(u.Paths.project_saved_dir(), 'Verification')
backup = saved / 'BeforeFirstRoomStartRepair'
backup.mkdir(parents=True, exist_ok=True)
source = project / 'Plugins/HorrorSystems/Content/Maps/Basic_roomA.umap'
if not (backup / source.name).exists():
    shutil.copy2(source, backup / source.name)
levels = u.get_editor_subsystem(u.LevelEditorSubsystem)
actors = u.get_editor_subsystem(u.EditorActorSubsystem)
assert levels.load_level('/HorrorSystems/Maps/Basic_roomA')
all_actors = list(actors.get_all_level_actors())
references = configure_first_room_references('Basic_roomA', all_actors)
assert references, 'Expected overlapping reference shell was not found'
starts = [a for a in all_actors if isinstance(a, u.PlayerStart) and str(a.player_start_tag) == 'Safe_A']
assert len(starts) == 1
starts[0].set_actor_location(u.Vector(-200, -1020, 292), False, False)
assert levels.save_current_level()
(saved / 'FirstRoomStartRepair.json').write_text(json.dumps({'map': 'Basic_roomA', 'editor_only_shells': references, 'spawn': [-200, -1020, 292]}, indent=2), encoding='utf-8')
u.log('FIRST_ROOM_START_REPAIRED')
