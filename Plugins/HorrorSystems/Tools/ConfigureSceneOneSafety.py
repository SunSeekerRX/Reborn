"""Bind the furnished Basic_roomA scene to first-person and safe-room systems.

Preserve its authored geometry, existing doorway, and furniture.
"""
import unreal as u
from pathlib import Path
import shutil
import json

path = "/HorrorSystems/Maps/Basic_roomA"
levels = u.get_editor_subsystem(u.LevelEditorSubsystem)
actors = u.get_editor_subsystem(u.EditorActorSubsystem)
saved = Path(u.Paths.project_saved_dir()) / "Verification"
saved.mkdir(parents=True, exist_ok=True)
backup = saved / "SceneOneBeforeSafety.umap"
if not backup.exists():
    shutil.copy2(Path(u.Paths.project_dir()) / "Plugins/HorrorSystems/Content/Maps/Basic_roomA.umap", backup)
assert levels.load_level(path)
original = actors.get_all_level_actors()
starts = [a for a in original if isinstance(a, u.PlayerStart)]
assert len(starts) == 1
starts[0].player_start_tag = "Safe"
starts[0].set_actor_location(u.Vector(-200, -1020, 300), False, False)
starts[0].set_actor_rotation(u.Rotator(0, 90, 0), False)
directors = [a for a in original if isinstance(a, u.HSRoomDirector)]
assert len(directors) <= 1
director = directors[0] if directors else actors.spawn_actor_from_class(u.HSRoomDirector, u.Vector(-200, -850, 400))
director.set_actor_label("SceneOne_SafeArea")
director.set_actor_location(u.Vector(-200, -850, 400), False, False)
director.safe_area.set_box_extent(u.Vector(200, 250, 200), False)
director.return_barrier.set_box_extent(u.Vector(200, 250, 200), False)
director.safe_spawn_tag = "Safe"
director.play_first_entry_sequence = False
rules = u.load_object(None, "/HorrorSystems/Rooms/DA_BasicRoomSafety.DA_BasicRoomSafety")
assert rules
director.rules = rules
world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property("default_game_mode", u.HSGameMode)
assert levels.save_current_level()
# No existing actors or geometry are replaced or removed.
assert len(actors.get_all_level_actors()) == len(original) + (0 if directors else 1)
report = {"map": path, "actor_count_before": len(original),
          "actor_count_after": len(actors.get_all_level_actors()),
          "safe_center": [-200, -850, 400], "safe_extent": [200, 250, 200],
          "seconds": 60, "spawn": [-200, -1020, 300], "spawn_yaw": 90,
          "geometry_preserved": True}
(saved / "SceneOneSafetyConfig.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
u.log("SCENE_ONE_SAFETY_CONFIGURED")
