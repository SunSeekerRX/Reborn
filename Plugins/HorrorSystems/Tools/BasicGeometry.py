"""Open Basic room walls while preserving the actual authored floor."""
import unreal as u

def cut_basic_room_openings(source_mesh,target_mesh,shell_transform,safe_exit_y,window_y,nominal_floor_z):
    dm=u.DynamicMesh()
    _,outcome=u.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(source_mesh,dm,u.GeometryScriptCopyMeshFromAssetOptions(),u.GeometryScriptMeshReadLOD())
    assert outcome==u.GeometryScriptOutcomePins.SUCCESS
    # Editor commandlets do not have a live physics world. Query the original
    # geometry directly, before any wall subtraction, to find the walking surface.
    _,bvh=u.GeometryScript_MeshSpatial.build_bvh_for_mesh(dm)
    sign=1 if window_y>safe_exit_y else -1
    inverse=u.MathLibrary.invert_transform(shell_transform)
    origin=u.MathLibrary.transform_location(inverse,u.Vector(-200,safe_exit_y-sign*200,nominal_floor_z+450))
    direction=u.MathLibrary.transform_direction(inverse,u.Vector(0,0,-1))
    _,hit,outcome=u.GeometryScript_MeshSpatial.find_nearest_ray_intersection_with_mesh(dm,bvh,origin,direction,u.GeometryScriptSpatialQueryOptions())
    assert hit.hit,'Original safe-room floor not found; refusing to cut below it'
    floor_z=u.MathLibrary.transform_location(shell_transform,hit.hit_position).z
    # One centimetre above the floor avoids coplanar booleans removing its top face.
    door_bottom=floor_z+1.0
    door_top=max(door_bottom+300.0,nominal_floor_z+500.0)
    cuts=[((-200,safe_exit_y,(door_bottom+door_top)*.5),(170,220,door_top-door_bottom)),
          ((50,window_y,nominal_floor_z+220),(220,320,280))]
    for center,size in cuts:
        cutter=u.DynamicMesh()
        u.GeometryScript_Primitives.append_box(cutter,u.GeometryScriptPrimitiveOptions(),u.Transform(location=u.Vector(*center)),*size,origin=u.GeometryScriptPrimitiveOriginMode.CENTER)
        u.GeometryScript_MeshBooleans.apply_mesh_boolean(dm,shell_transform,cutter,u.Transform(),u.GeometryScriptBooleanOperation.SUBTRACT,u.GeometryScriptMeshBooleanOptions())
    _,outcome=u.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(dm,target_mesh,u.GeometryScriptCopyMeshToAssetOptions(),u.GeometryScriptMeshWriteLOD())
    assert outcome==u.GeometryScriptOutcomePins.SUCCESS
    target_mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    u.EditorAssetLibrary.set_metadata_tag(target_mesh,'OriginalShellMesh',source_mesh.get_path_name())
    assert u.EditorAssetLibrary.save_loaded_asset(target_mesh)
    return {'source':source_mesh.get_path_name(),'target':target_mesh.get_path_name(),'floor_z':floor_z,'door_bottom':door_bottom,'exit_y':safe_exit_y}
