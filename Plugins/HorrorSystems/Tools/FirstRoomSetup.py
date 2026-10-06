"""Keep the extra standalone-A room shell as an editor-only reference."""
import unreal as u

def configure_first_room_references(map_name, actors):
    if map_name != 'Basic_roomA':
        return []
    references = []
    for actor in actors:
        if isinstance(actor,u.SkeletalMeshActor) and actor.get_actor_label()=='SK_Mannequin':
            actor.set_actor_hidden_in_game(True)
            actor.set_actor_enable_collision(False)
            actor.skeletal_mesh_component.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
            actor.set_editor_property('is_editor_only_actor',True)
        if isinstance(actor, u.StaticMeshActor) and actor.get_actor_label().startswith('CubeGridToolOutput2'):
            actor.set_actor_hidden_in_game(True)
            actor.set_actor_enable_collision(False)
            actor.static_mesh_component.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
            actor.set_editor_property('is_editor_only_actor', True)
            references.append(actor.get_actor_label())
    return references
