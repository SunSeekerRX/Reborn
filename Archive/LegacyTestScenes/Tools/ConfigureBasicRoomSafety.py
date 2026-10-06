"""Connect both Basic_room maps to the existing safe-room countdown system.

Run in UE with GeometryScripting enabled. Keep the original whitebox mesh intact.
"""
import unreal as u
from pathlib import Path
import json
import shutil

root = "/HorrorSystems"
maps = [root + "/Maps/Basic_room", "/Game/Basic_room"]
eal = u.EditorAssetLibrary
levels = u.get_editor_subsystem(u.LevelEditorSubsystem)
actors = u.get_editor_subsystem(u.EditorActorSubsystem)
saved = Path(u.Paths.project_saved_dir()) / "Verification" / "BasicRoomBeforeSafety"
saved.mkdir(parents=True, exist_ok=True)
project = Path(u.Paths.project_dir())
for relative in ("Content/Basic_room.umap", "Plugins/HorrorSystems/Content/Maps/Basic_room.umap"):
    source = project / relative
    backup = saved / relative
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        shutil.copy2(source, backup)

source_mesh = root + "/Maps/_GENERATED/doubl/CubeGridToolOutput_CDA36625"
mesh_path = root + "/Rooms/SM_BasicRoomSafeExit"
u.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([root, "/Game"], force_rescan=True)
mesh = eal.load_asset(mesh_path) if eal.does_asset_exist(mesh_path) else None
if not mesh:
    assert levels.load_level(maps[0])
    original = next(a for a in actors.get_all_level_actors() if a.get_actor_label() == "CubeGridToolOutput")
    original_mesh = original.get_component_by_class(u.StaticMeshComponent).static_mesh
    mesh = u.AssetToolsHelpers.get_asset_tools().duplicate_asset("SM_BasicRoomSafeExit", root + "/Rooms", original_mesh)
    assert mesh, "Cannot copy whitebox mesh"
    dynamic = u.DynamicMesh()
    _, outcome = u.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(
        mesh, dynamic, u.GeometryScriptCopyMeshFromAssetOptions(), u.GeometryScriptMeshReadLOD())
    assert outcome == u.GeometryScriptOutcomePins.SUCCESS
    cutter = u.DynamicMesh()
    # Local coordinates: front wall at world Y=-600..-500; floor Z=100.
    u.GeometryScript_Primitives.append_box(cutter, u.GeometryScriptPrimitiveOptions(),
        u.Transform(location=u.Vector(1000, -450, 100)), 160, 220, 300)
    u.GeometryScript_MeshBooleans.apply_mesh_boolean(dynamic, u.Transform(), cutter,
        u.Transform(), u.GeometryScriptBooleanOperation.SUBTRACT, u.GeometryScriptMeshBooleanOptions())
    _, outcome = u.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(
        dynamic, mesh, u.GeometryScriptCopyMeshToAssetOptions(), u.GeometryScriptMeshWriteLOD())
    assert outcome == u.GeometryScriptOutcomePins.SUCCESS
    body = mesh.get_editor_property("body_setup")
    body.set_editor_property("collision_trace_flag", u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    assert eal.save_loaded_asset(mesh)

rules_path = root + "/Rooms/DA_BasicRoomSafety"
rules = eal.load_asset(rules_path) if eal.does_asset_exist(rules_path) else None
if not rules:
    factory = u.DataAssetFactory()
    factory.set_editor_property("data_asset_class", u.HSRoomRules)
    rules = u.AssetToolsHelpers.get_asset_tools().create_asset(
        "DA_BasicRoomSafety", root + "/Rooms", u.HSRoomRules, factory)
rules.set_editor_property("room_id", "BasicRoom")
rules.set_editor_property("room_name", "房间")
rules.set_editor_property("duration", 60.0)
assert eal.save_loaded_asset(rules)

report = []
for path in maps:
    assert levels.load_level(path), path
    all_actors = actors.get_all_level_actors()
    starts = [a for a in all_actors if isinstance(a, u.PlayerStart)]
    assert len(starts) == 1, "Expected one spawn point"
    starts[0].player_start_tag = "Safe"
    starts[0].set_actor_location(u.Vector(-200, -1020, 200), False, False)
    whitebox = next(a for a in all_actors if a.get_actor_label() == "CubeGridToolOutput")
    whitebox.get_component_by_class(u.StaticMeshComponent).set_static_mesh(mesh)
    directors = [a for a in all_actors if isinstance(a, u.HSRoomDirector)]
    assert len(directors) <= 1
    director = directors[0] if directors else actors.spawn_actor_from_class(
        u.HSRoomDirector, u.Vector(-200, -850, 300))
    director.set_actor_location(u.Vector(-200, -850, 300), False, False)
    director.set_actor_label("BasicRoom_SafeArea")
    director.rules = rules
    director.safe_spawn_tag = "Safe"
    director.play_first_entry_sequence = False
    director.safe_area.set_box_extent(u.Vector(200, 250, 200), False)
    director.return_barrier.set_box_extent(u.Vector(200, 250, 200), False)
    world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property("default_game_mode", u.HSGameMode)
    assert levels.save_current_level(), path
    report.append({"map": path, "seconds": 60, "safe_center": [-200, -850, 300],
        "safe_extent": [200, 250, 200], "spawn": [-200, -1020, 200],
        "spawn_tag": "Safe", "door_width_cm": 160, "door_height_cm": 300})
target = Path(u.Paths.project_saved_dir()) / "Verification" / "BasicRoomSafetyConfig.json"
target.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
u.log("BASIC_ROOM_SAFETY_CONFIGURED")
