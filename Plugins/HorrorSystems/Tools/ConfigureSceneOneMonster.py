"""Add one pursuing monster and navigation to the furnished first room."""
import unreal as u
from pathlib import Path
import shutil
levels = u.get_editor_subsystem(u.LevelEditorSubsystem)
actors = u.get_editor_subsystem(u.EditorActorSubsystem)
saved = Path(u.Paths.project_saved_dir()) / "Verification"
backup = saved / "SceneOneBeforeMonster.umap"
if not backup.exists():
    shutil.copy2(Path(u.Paths.project_dir()) / "Plugins/HorrorSystems/Content/Maps/Basic_roomA.umap", backup)
assert levels.load_level("/HorrorSystems/Maps/Basic_roomA")
all_actors = actors.get_all_level_actors()
# The original static mannequin is a preview prop, not a gameplay pawn.
# Its skeletal collision is absent from the static navigation mesh.
for actor in all_actors:
    if isinstance(actor, u.SkeletalMeshActor):
        component = actor.get_component_by_class(u.SkeletalMeshComponent)
        component.set_collision_response_to_channel(u.CollisionChannel.ECC_PAWN, u.CollisionResponseType.ECR_IGNORE)
        u.log("PREVIEW_SKELETAL_PROP_PAWN_IGNORE: " + actor.get_name())
monsters = [a for a in all_actors if isinstance(a, u.HSMonster) and not a.cinematic_actor]
monster = monsters[0] if monsters else actors.spawn_actor_from_class(u.HSMonster, u.Vector(-850, 1100, 300))
monster.set_actor_label("SceneOne_PursuingMonster")
monster.can_damage_player = True
monster.automatic_speed = True
monster.stagger_duration = 5.0
monster.stagger_speed = 30.0
navs = [a for a in all_actors if isinstance(a, u.NavMeshBoundsVolume)]
nav = navs[0] if navs else actors.spawn_actor_from_class(u.NavMeshBoundsVolume, u.Vector(-550, 200, 450))
nav.set_actor_label("SceneOne_NavigationBounds")
nav.set_actor_location(u.Vector(-550, 200, 450), False, False)
nav.set_actor_scale3d(u.Vector(10, 17, 6))
assert levels.save_current_level()
u.log("SCENE_ONE_MONSTER_CONFIGURED")
