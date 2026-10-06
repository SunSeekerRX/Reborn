"""Bind the authored rooms to Game Jam v0.1 sections 9-11, preserving their geometry."""
import os
import json
from pathlib import Path
import unreal as u

ROOT = '/HorrorSystems'
out = Path(os.environ.get('REBORN_VERIFICATION_DIR', str(Path(u.Paths.project_dir()).parent / 'LocalArtifacts/GameJam_20261007')))
out.mkdir(parents=True, exist_ok=True)
EAL = u.EditorAssetLibrary
tools = u.AssetToolsHelpers.get_asset_tools()
actors = u.get_editor_subsystem(u.EditorActorSubsystem)
levels = u.get_editor_subsystem(u.LevelEditorSubsystem)
registry = u.AssetRegistryHelpers.get_asset_registry()
registry.scan_paths_synchronous([ROOT], force_rescan=True)

def load(path):
    asset = EAL.load_asset(path)
    assert asset, path
    return asset

def spawn(cls, location, label):
    actor = actors.spawn_actor_from_class(cls, u.Vector(*location))
    assert actor, label
    actor.set_actor_label(label)
    actor.set_editor_property('tags', [u.Name('Reborn_Jam')])
    return actor

def make_data(name, cls):
    path = ROOT + '/Story/' + name
    if EAL.does_asset_exist(path):
        return load(path)
    factory = u.DataAssetFactory()
    factory.set_editor_property('data_asset_class', cls)
    return tools.create_asset(name, ROOT + '/Story', cls, factory)

def material(name, color, emissive=False):
    path = ROOT + '/Materials/Jam/' + name
    mat = load(path) if EAL.does_asset_exist(path) else tools.create_asset(name, ROOT + '/Materials/Jam', u.Material, u.MaterialFactoryNew())
    mat.set_editor_property('used_with_nanite', True)
    mel = u.MaterialEditingLibrary
    mel.delete_all_material_expressions(mat)
    node = mel.create_material_expression(mat, u.MaterialExpressionConstant3Vector, 0, 0)
    node.set_editor_property('constant', u.LinearColor(*color, 1))
    if emissive:
        strength = mel.create_material_expression(mat, u.MaterialExpressionScalarParameter, 0, 160)
        strength.set_editor_property('parameter_name','LightStrength')
        strength.set_editor_property('default_value',1)
        multiply = mel.create_material_expression(mat, u.MaterialExpressionMultiply, 160, 0)
        mel.connect_material_expressions(node, '', multiply, 'A')
        mel.connect_material_expressions(strength, '', multiply, 'B')
        mel.connect_material_property(multiply, '', u.MaterialProperty.MP_EMISSIVE_COLOR)
    else:
        mel.connect_material_property(node, '', u.MaterialProperty.MP_BASE_COLOR)
    mat.set_editor_property('two_sided', True)
    mel.recompile_material(mat)
    assert EAL.save_loaded_asset(mat)
    return mat

blank = material('M_BlankCanvas', (.32, .3, .28))
bulb = material('M_FlickeringBulb', (1.2, .8, .35), True)
metal = material('M_LampHousing', (.045, .04, .035))
rules = {c: make_data('DA_JamRoom' + c, u.HSRoomRules) for c in 'ABC'}
route_specs = {
    'A': [(1, 0, 'B', 1, ['Photo_A'], False), (2, 2, 'C', 3, [], False),
          (2, 5, 'A', 6, ['Clue_A'], True)],
    'B': [(1, 1, 'A', 2, ['Warning_B'], True), (2, 4, 'A', 5, ['Clue_B'], False)],
    'C': [(2, 3, 'B', 4, ['Testament_C'], False)],
}
for c, rule in rules.items():
    rule.set_editor_property('room_id', 'Room' + c)
    rule.set_editor_property('room_name', '房间 ' + c)
    rule.set_editor_property('duration', 60)
    routes = []
    for stage, step, target, next_step, required, advance in route_specs[c]:
        route = u.HSRoomRoute()
        route.set_editor_property('stage', stage)
        route.set_editor_property('required_story_step', step)
        route.set_editor_property('next_story_step', next_step)
        route.set_editor_property('target_room', 'Room' + target)
        route.set_editor_property('destination', load(ROOT + '/Maps/' + ('Basic_roomABC_unchange1' if next_step == 6 else 'Basic_room' + target)))
        route.set_editor_property('required_clues', required)
        route.set_editor_property('advance_stage', advance)
        route.set_editor_property('require_all_stage2_clues', step == 5)
        routes.append(route)
    final = u.HSRoomRoute()
    final.set_editor_property('stage', 3)
    final.set_editor_property('required_story_step', 6)
    final.set_editor_property('next_story_step', 7)
    final.set_editor_property('target_room', 'Exit')
    final.set_editor_property('destination', load(ROOT + '/Maps/RebornTitle'))
    final.set_editor_property('required_clues', ['FinalMessage'])
    final.set_editor_property('finish_at_final_stage', True)
    routes.append(final)
    rule.set_editor_property('routes', routes)
    rule.set_editor_property('clue_labels', {'Photo_A': '照片：“快逃！入口就是出口！”', 'Warning_B': '信息：“房间 3 才是逃离的关键。”', 'Testament_C': '遗言：“第三扇门后，我第一次看见外面的光。”', 'Clue_B': '“第二间房不是出口。”', 'Clue_A': '“我又回到了第一次醒来的地方。”', 'FinalMessage': '“门已经打开，别回头。”'})
    assert EAL.save_loaded_asset(rule)

paper_template = load(ROOT + '/Items/DA_Note')
story_items = {}
item_specs = {
    'Photo_A': ('旧照片', '快逃！入口就是出口！\n\n照片背面的警告与一枚门钥匙，让你想起刚才经过的入口。', '照片背面写着……入口就是出口？'),
    'Warning_B': ('陌生人留下的信息', '房间 3 才是逃离的关键。\n\n这里还有一个奇怪的人，千万别让他抓到。\n\n纸条旁的钥匙能够重新打开安全屋木门。', '房间 3……这里还有别人？'),
    'Testament_C': ('房间 C 的遗言', '三个房间分别留下了一部分信息。把它们都找到，才能理解真正的逃生方法。\n\n第三扇门后，我第一次看见外面的光。', '还有三条信息……先找到它们。'),
    'Clue_B': ('房间 B 的线索', '第二间房不是出口。', '第二间房不是出口……还差最初的房间。'),
    'Clue_A': ('房间 A 的最后线索', '我又回到了第一次醒来的地方。\n\n三条信息拼在一起，指向了 A → B → C 的顺序。', '原来是这样……该按顺序走。'),
    'Introduction_A': ('第二次醒来的提示', '墙面出现了血字，家具也挪动了。\n\n去房间 3，那里留着真正离开的方法。', '这里变了……去房间 3 看看。'),
    'FinalMessage': ('最后的纸条', '门已经打开，别回头。\n\n这一枚钥匙可以打开沿途所有木门。A → B → C → 出口。', '门已经打开……别回头。'),
}
for item_id, (title, description, subtitle) in item_specs.items():
    path = ROOT + '/Story/DA_' + item_id
    item = load(path) if EAL.does_asset_exist(path) else EAL.duplicate_asset(ROOT + '/Items/DA_Note', path)
    item.set_editor_property('item_id', item_id)
    item.set_editor_property('display_name', title)
    item.set_editor_property('description', description)
    if item_id=='Clue_A': item.set_editor_property('description',description+'\n\n纸张背面：门已经打开，别回头。')
    item.set_editor_property('pickup_subtitle', subtitle)
    item.set_editor_property('inspect_on_pickup', False)
    item.set_editor_property('pickup_sound', load(ROOT + '/Audio/Jam/S_Pickup'))
    assert item.inspection_mesh, item_id
    assert EAL.save_loaded_asset(item)
    story_items[item_id] = item

def copy_mesh(old, new):
    src = old.get_component_by_class(u.StaticMeshComponent)
    dest = new.get_component_by_class(u.StaticMeshComponent)
    assert src and dest and src.static_mesh
    new.set_actor_transform(src.get_world_transform(), False, False)
    dest.set_static_mesh(src.static_mesh)
    for i in range(src.get_num_materials()):
        dest.set_material(i, src.get_material(i))
    new.set_editor_property('tags', list(old.tags) + [u.Name('Reborn_Jam')])
    actors.destroy_actor(old)
    return new

door_mesh = load(ROOT + '/Art/Saferoomdoor/Door/SM_Saferoom_door')
frame_mesh = load(ROOT + '/Art/Saferoomdoor/Outside/SM_Saferoom_outside')
frame_mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag', u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
assert EAL.save_loaded_asset(frame_mesh)

def door(old, label, point=None, direction=1):
    d = spawn(u.HSWoodDoor, point or (0, 0, 0), label)
    if old:
        copy_mesh(old, d)
    else:
        d.mesh.set_static_mesh(door_mesh)
        d.set_actor_scale3d(u.Vector(7.7, 5, 5.3))
        f = spawn(u.StaticMeshActor, (point[0], point[1] + 2.4, point[2] + 148.8), label + '_Frame')
        f.static_mesh_component.set_static_mesh(frame_mesh)
        f.set_actor_scale3d(u.Vector(7.6, 6, 5.3))
        f.static_mesh_component.set_collision_profile_name('BlockAll')
    bounds = door_mesh.get_bounds()
    d.set_editor_property('hinge_offset', u.Vector(bounds.origin.x - bounds.box_extent.x, bounds.origin.y, bounds.origin.z - bounds.box_extent.z))
    d.set_editor_property('open_angle', 100 * direction)
    d.set_editor_property('open_sound', load(ROOT + '/Audio/Jam/S_DoorOpen'))
    d.set_editor_property('close_sound', load(ROOT + '/Audio/Jam/S_DoorClose'))
    d.mesh.set_mobility(u.ComponentMobility.MOVABLE)
    return d

def seat_item(item_id, step, stage, table, label):
    p = spawn(u.HSPickup, (0, 0, 0), label)
    item = story_items[item_id]
    p.set_editor_property('item_data', item)
    p.set_editor_property('pickup_id', item_id)
    p.set_editor_property('clue_id', item_id)
    p.set_editor_property('required_story_step', step)
    p.set_editor_property('minimum_stage', stage); p.set_editor_property('maximum_stage', stage)
    p.set_editor_property('rotate_for_demo', False)
    p.mesh.set_static_mesh(item.inspection_mesh)
    for i, mat in enumerate(item.inspection_materials):
        p.mesh.set_material(i, mat)
    # Lay the thin paper horizontally and normalize it to a handheld size.
    p.set_actor_rotation(u.Rotator(pitch=90, yaw=0, roll=0), False)
    p.set_actor_scale3d(u.Vector(.7, .7, .7))
    mb = p.mesh.static_mesh.get_bounds()
    mt = p.mesh.get_world_transform()
    verts = [mt.transform_location(mb.origin + u.Vector(i*mb.box_extent.x,j*mb.box_extent.y,k*mb.box_extent.z)) for i in (-1,1) for j in (-1,1) for k in (-1,1)]
    extent = u.Vector((max(v.x for v in verts)-min(v.x for v in verts))/2,(max(v.y for v in verts)-min(v.y for v in verts))/2,0)
    factor = 13 / max(extent.x, extent.y)
    p.set_actor_scale3d(p.get_actor_scale3d() * factor)
    component = table.get_component_by_class(u.StaticMeshComponent)
    dm = u.DynamicMesh()
    _, result = u.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(component.static_mesh, dm, u.GeometryScriptCopyMeshFromAssetOptions(), u.GeometryScriptMeshReadLOD())
    assert result == u.GeometryScriptOutcomePins.SUCCESS
    u.GeometryScript_MeshTransforms.transform_mesh(dm, component.get_world_transform())
    _, bvh = u.GeometryScript_MeshSpatial.build_bvh_for_mesh(dm)
    tc, te = table.get_actor_bounds(False)
    x, y = tc.x + te.x * .15, tc.y - te.y * .2
    _, hit, _ = u.GeometryScript_MeshSpatial.find_nearest_ray_intersection_with_mesh(dm, bvh, u.Vector(x, y, tc.z + te.z + 100), u.Vector(0, 0, -1), u.GeometryScriptSpatialQueryOptions())
    assert hit.hit, table.get_actor_label()
    # Actor bounds include the hidden old label; use mesh bounds only.
    b = p.mesh.static_mesh.get_bounds()
    t = p.mesh.get_world_transform()
    corners = [t.transform_location(b.origin + u.Vector(i*b.box_extent.x, j*b.box_extent.y, k*b.box_extent.z)) for i in (-1,1) for j in (-1,1) for k in (-1,1)]
    bottom = min(v.z for v in corners)
    p.set_actor_location(u.Vector(x, y, hit.hit_position.z + .4 - bottom), False, False)
    p.mesh.set_collision_profile_name('BlockAllDynamic')
    return p

report = []
for name in ['Basic_roomA', 'Basic_roomB', 'Basic_roomC', 'Basic_roomABC_unchange1']:
    assert levels.load_level(ROOT + '/Maps/' + name)
    existing = list(actors.get_all_level_actors())
    # Read actual floor triangles instead of assuming that all safe rooms share a height.
    surfaces = []
    for a in existing:
        if not isinstance(a,u.StaticMeshActor): continue
        mesh = a.static_mesh_component
        if not mesh.static_mesh: continue
        center,extent = a.get_actor_bounds(False)
        if extent.x < 130 or extent.y < 130: continue
        dm = u.DynamicMesh()
        _, result = u.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(mesh.static_mesh,dm,u.GeometryScriptCopyMeshFromAssetOptions(),u.GeometryScriptMeshReadLOD())
        if result != u.GeometryScriptOutcomePins.SUCCESS: continue
        u.GeometryScript_MeshTransforms.transform_mesh(dm,mesh.get_world_transform())
        _, bvh = u.GeometryScript_MeshSpatial.build_bvh_for_mesh(dm)
        surfaces.append((dm,bvh,center,extent))
    def floor_at(x,y,fallback=100):
        hits=[]
        for dm,bvh,center,extent in surfaces:
            if abs(x-center.x)>extent.x+1 or abs(y-center.y)>extent.y+1: continue
            _,hit,_ = u.GeometryScript_MeshSpatial.find_nearest_ray_intersection_with_mesh(dm,bvh,u.Vector(x,y,350),u.Vector(0,0,-1),u.GeometryScriptSpatialQueryOptions())
            if hit.hit and hit.hit_position.z < 300: hits.append(hit.hit_position.z)
        assert hits, 'No floor at '+str((name,x,y))
        return max(hits)
    combined = name == 'Basic_roomABC_unchange1'
    letter = 'A' if combined else name[-1]
    old_door = next((a for a in existing if a.get_actor_label() in ('SafeDoor','Jam_WoodDoor_'+letter)), None) if not combined else None
    old_window = next((a for a in existing if isinstance(a, u.HSWindowSequence)), None)
    for actor in existing:
        if actor==old_door or isinstance(actor,(u.HSVisitPainting,u.HSSwapPainting)): continue
        if isinstance(actor, (u.HSPickup, u.HSInspectTrigger, u.HSRoomDirector, u.HSPortal, u.HSMonster, u.HSRecoveryCheckpoint, u.HSWoodDoor, u.HSFlickerLamp)):
            actors.destroy_actor(actor)
        elif 'Jam_' in actor.get_actor_label():
            actors.destroy_actor(actor)
        elif isinstance(actor, u.SkeletalMeshActor):
            actor.set_editor_property('is_editor_only_actor', True)
            actor.set_actor_hidden_in_game(True)
            actor.set_actor_enable_collision(False)
    existing = list(actors.get_all_level_actors())
    scope = 'ABC' if combined else letter
    directors = {}
    doors = {}
    for c in scope:
        safe_y = -955 if not combined or c == 'A' else -1550 if c == 'B' else -5547
        floor = floor_at(-200,safe_y)
        d = spawn(u.HSStoryDirector, (-200, safe_y, floor+110), 'Reborn_SafeRoom_' + c)
        d.set_editor_property('rules', rules[c])
        d.set_editor_property('safe_spawn_tag', 'Safe_' + c)
        d.set_editor_property('final_escape_mode', combined)
        d.set_editor_property('minimum_stage', 3 if combined else 1)
        d.set_editor_property('use_room_bounds', combined)
        d.safe_area.set_box_extent(u.Vector(210, 275 if c == 'A' or not combined else 140, 210), False)
        if combined:
            low, high = ( -1200, 1500 ) if c == 'A' else ( -5200, -1200 ) if c == 'B' else ( -9000, -5200 )
            d.room_bounds.set_world_location(u.Vector(-550, (low+high)/2, 350), False, False)
            d.room_bounds.set_box_extent(u.Vector(650, (high-low)/2, 500), False)
        starts = [a for a in actors.get_all_level_actors() if isinstance(a, u.PlayerStart) and (str(a.player_start_tag) == 'Safe_' + c or not combined)]
        start = starts[0] if starts else spawn(u.PlayerStart, (-200, safe_y, floor+92), 'Reborn_SafeStart_' + c)
        start.set_editor_property('player_start_tag', 'Safe_' + c)
        start.set_actor_location(u.Vector(-200, safe_y, floor+92), False, False)
        start.set_actor_rotation(u.Rotator(pitch=0, yaw=90 if not combined or c=='A' else -90, roll=0), False)
        door_y = -650 if not combined or c=='A' else -1700 if c=='B' else -5697
        point = (-200,door_y,floor_at(-200,door_y))
        gate = door(old_door if not combined else None, 'Jam_WoodDoor_' + c, point, 1 if not combined or c=='A' else -1)
        gate.set_editor_property('requires_final_message', combined and c != 'A')
        d.set_editor_property('safe_door', gate)
        directors[c], doors[c] = d, gate
        if not combined:
            portal = spawn(u.HSPortal, (-200, -1060, 350), 'Reborn_SafePortal_' + c)
            portal.set_editor_property('use_stage_route', True); portal.set_editor_property('requires_safe_return', True); portal.set_editor_property('white_light_travel', True)
            portal.set_editor_property('room_rules', rules[c])
            portal.set_editor_property('occluding_door', gate)
            portal.marker.set_material(0, load(ROOT + '/Materials/M_SafeWhitePortal'))
            portal.marker.set_relative_scale3d(u.Vector(.08, 1.7, 2.6))
            portal.set_actor_rotation(u.Rotator(pitch=0, yaw=90, roll=0), False)
            portal.trigger.set_box_extent(u.Vector(20, 80, 135), False)
            portal.portal_light.set_editor_property('cast_shadows', True)
            portal.portal_light.set_attenuation_radius(200)
            portal.set_editor_property('travel_sound', load(ROOT + '/Audio/Jam/S_WhiteFlash'))
        mx = -200 if combined and c in 'BC' else -850
        # Place C before the final entrance threshold, on the connected ramp toward the main room.
        my = 1100 if not combined or c=='A' else -2200 if c=='B' else -8300
        monster = spawn(u.HSMonster, (mx,my,floor_at(mx,my)+92), 'Jam_Monster_' + c)
        monster.set_editor_property('home_room', 'Room' + c)
        monster.set_editor_property('can_damage_player', True)
        monster.set_editor_property('use_story_pressure', True)
        monster.set_editor_property('presence_sound', load(ROOT + '/Audio/Jam/S_MonsterLaugh'))
        monster.set_editor_property('footstep_sound', load(ROOT + '/Audio/Jam/S_MonsterStepSingle'))
        d.set_editor_property('pursuer', monster)
        d.set_editor_property('monster_trigger_point', u.Vector(-200,-2450,192) if combined and c=='B' else u.Vector(-550,-7550,192))
        d.set_editor_property('monster_trigger_radius', 700)
    world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property('default_game_mode', u.HSGameMode)
    if not combined and letter == 'B':
        window = old_window or spawn(u.HSWindowSequence, (0,850,0), 'Reborn_Window_B')
        window.set_editor_property('camera_offset', u.Vector(-100, 0, 350))
        window.set_editor_property('camera_waypoints', [u.Vector(-400, -350, 350), u.Vector(-160, -100, 350)])
        window.set_editor_property('window_view_rotation', u.Rotator(pitch=0, yaw=0, roll=0))
        window.set_editor_property('monster_start', u.Vector(180, -280, 272))
        window.set_editor_property('monster_look_point', u.Vector(140, 0, 272))
        window.set_editor_property('monster_end', u.Vector(180, 650, 272))
        directors['B'].set_editor_property('window_sequence', window)
    tables = [a for a in actors.get_all_level_actors() if 'table' in a.get_actor_label().lower() and a.get_component_by_class(u.StaticMeshComponent)]
    assert tables, name
    for table_actor in tables:
        table_mesh=table_actor.get_component_by_class(u.StaticMeshComponent).static_mesh
        collision_mesh=u.DynamicMesh()
        u.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(table_mesh,collision_mesh,u.GeometryScriptCopyMeshFromAssetOptions(),u.GeometryScriptMeshReadLOD())
        options=u.GeometryScriptCollisionFromMeshOptions()
        options.method=u.GeometryScriptCollisionGenerationMethod.ALIGNED_BOXES
        u.GeometryScript_Collision.set_static_mesh_collision_from_mesh(collision_mesh,table_mesh,options)
        table_mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',u.CollisionTraceFlag.CTF_USE_SIMPLE_AND_COMPLEX)
        assert EAL.save_loaded_asset(table_mesh)
    table = max(tables, key=lambda a:a.get_actor_bounds(False)[0].y) if not combined else min(tables, key=lambda a:abs(a.get_actor_bounds(False)[0].y-750))
    specs = [('FinalMessage',6,3)] if combined else [('Photo_A',0,1),('Clue_A',5,2)] if letter=='A' else [('Warning_B',1,1),('Clue_B',4,2)] if letter=='B' else [('Testament_C',3,2)]
    for item_id, step, stage in specs:
        seat_item(item_id, step, stage, table, 'Jam_' + item_id)
    if name == 'Basic_roomA':
        variation=spawn(u.HSRoomVariation,(-700,1480,380),'Jam_SecondVisitChanges')
        variation.set_actor_rotation(u.Rotator(pitch=0,yaw=-90,roll=0),False)
        changed_table=min(tables,key=lambda a:abs(a.get_actor_location().y-743))
        changed_table.static_mesh_component.set_mobility(u.ComponentMobility.MOVABLE)
        variation.set_editor_property('furniture',changed_table)
        intro=seat_item('Introduction_A',2,2,table,'Jam_Introduction_A')
        intro.set_editor_property('clue_id',u.Name('None'))
        for old in list(actors.get_all_level_actors()):
            if old.get_actor_label() in ('Drawing','Drawing002'):
                painting = old if isinstance(old,u.HSVisitPainting) else copy_mesh(old,spawn(u.HSVisitPainting,(0,0,0),old.get_actor_label()))
                painting.set_editor_property('blank_material', blank)
    if name == 'Basic_roomB':
        for old in list(actors.get_all_level_actors()):
            if old.get_actor_label() in ('Drawing','Drawing2','Drawing3'):
                painting = old if isinstance(old,u.HSSwapPainting) else copy_mesh(old,spawn(u.HSSwapPainting,(0,0,0),old.get_actor_label()))
                painting.set_editor_property('swap_group', 'RoomB_Paintings')
                painting.set_editor_property('swap_sound', load(ROOT + '/Audio/Jam/S_CabinetDrag'))
    if combined:
        # Leave space on both sides of the C table for the final bait-and-circle encounter.
        lure = min(tables,key=lambda a:abs(a.get_actor_location().y+7179))
        lure.set_actor_scale3d(u.Vector(2.3,2.3,1))
        lure.set_actor_location(u.Vector(-550,-7179,lure.get_actor_location().z),False,False)
        lure.set_actor_label('LargeTable_C_BaitLoop')
        obstacles = [a for a in actors.get_all_level_actors() if isinstance(a,u.HSCollapsingObstacle)]
        source_book=min(obstacles,key=lambda a:abs(a.get_actor_location().y+2620))
        for n,(x,half_width) in enumerate([(-900,200),(-250,250)]):
            choke=spawn(u.StaticMeshActor,(0,0,0),'Jam_B_NarrowBookshelf_'+str(n))
            sm=choke.static_mesh_component;sm.set_static_mesh(source_book.mesh.static_mesh)
            for i in range(source_book.mesh.get_num_materials()): sm.set_material(i,source_book.mesh.get_material(i))
            choke.set_actor_transform(source_book.mesh.get_world_transform(),False,False)
            _,ext=choke.get_actor_bounds(False);scale=choke.get_actor_scale3d()
            choke.set_actor_scale3d(u.Vector(scale.x,scale.y*160/ext.y,scale.z*half_width/ext.x))
            center,_=choke.get_actor_bounds(False)
            choke.set_actor_location(choke.get_actor_location()+u.Vector(x,-3600,250)-center,False,False)
            sm.set_collision_profile_name('BlockAll')
        guide=spawn(u.HSFlickerLamp,(-600,-3600,480),'Jam_B_PathGuidance')
        guide.set_editor_property('emergency_guidance',True);guide.set_editor_property('base_intensity',25)
        for obstacle in obstacles:
            obstacle.set_editor_property('collapse_sound', load(ROOT + '/Audio/Jam/S_BookCollapse'))
            obstacle.set_editor_property('minimum_collapse_stage', 3)
            if obstacle.get_actor_location().y > -1200:
                directors['A'].set_editor_property('escape_obstacles', [obstacle])
        connector = door(None,'Jam_ConnectingDoor_AB',(-200,-1200,floor_at(-200,-1200)),-1)
        connector.set_editor_property('requires_final_message', True)
        final_door = door(None,'Jam_FinalWoodDoor',(-200,-8560,floor_at(-200,-8560)),-1)
        final_door.set_editor_property('requires_final_message', True)
        exit = spawn(u.HSPortal,(-200,-8890,250),'Reborn_FinalExit')
        exit.set_editor_property('room_rules', rules['C'])
        exit.set_editor_property('use_stage_route', True); exit.set_editor_property('white_light_travel', True)
        exit.set_editor_property('requires_safe_return', False)
        exit.set_editor_property('occluding_door', final_door)
        exit.marker.set_material(0,load(ROOT+'/Materials/M_SafeWhitePortal'))
        exit.set_actor_rotation(u.Rotator(pitch=0,yaw=90,roll=0),False)
        exit.set_editor_property('travel_sound', load(ROOT + '/Audio/Jam/S_WhiteFlash'))
        checkpoint = spawn(u.HSRecoveryCheckpoint,(-200,-8700,298),'Reborn_FinalSafety')
        checkpoint.set_editor_property('spawn_transform', u.Transform(location=u.Vector(-200, -8680, floor_at(-200, -8680) + 92), rotation=u.Rotator(pitch=0, yaw=-90, roll=0)))
    for c in scope:
        ys = (250,850,1350) if not combined or c=='A' else (-2300,-3400,-4400) if c=='B' else (-6200,-7300,-8100)
        for n,y in enumerate(ys):
            lamp = spawn(u.HSFlickerLamp,(-700 if c!='B' or not combined else -230,y,545),'Jam_CeilingLamp_'+c+'_'+str(n+1))
            lamp.set_editor_property('base_intensity', 45)
            lamp.set_editor_property('emergency_guidance', combined and c == 'B' and (n == 2))
            fixture = spawn(u.StaticMeshActor,(-700 if c!='B' or not combined else -230,y,555),'Jam_LampFixture_'+c+'_'+str(n+1))
            fixture.static_mesh_component.set_static_mesh(load('/Engine/BasicShapes/Sphere'))
            fixture.static_mesh_component.set_material(0,bulb)
            fixture.static_mesh_component.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
            fixture.set_actor_scale3d(u.Vector(.12,.12,.12))
            lamp.set_editor_property('fixture', fixture)
            stem = spawn(u.StaticMeshActor,(fixture.get_actor_location().x,y,580),'Jam_LampStem_'+c+'_'+str(n+1))
            stem.static_mesh_component.set_static_mesh(load('/Engine/BasicShapes/Cylinder'))
            stem.static_mesh_component.set_material(0,metal)
            stem.static_mesh_component.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
            stem.set_actor_scale3d(u.Vector(.02,.02,.4))
    if not any(isinstance(a,u.HSVisionRig) for a in actors.get_all_level_actors()): spawn(u.HSVisionRig,(0,0,0),'Reborn_Vision')
    if not any(isinstance(a,u.HSSceneAudio) for a in actors.get_all_level_actors()): spawn(u.HSSceneAudio,(0,0,0),'Reborn_SceneAudio')
    navs = [a for a in actors.get_all_level_actors() if isinstance(a,u.NavMeshBoundsVolume)]
    if not navs:
        nav = spawn(u.NavMeshBoundsVolume,(-550,-3700 if combined else 250,400),'Reborn_Navigation')
        nav.set_actor_scale3d(u.Vector(8,55 if combined else 17,6))
    for actor in actors.get_all_level_actors():
        for light in actor.get_components_by_class(u.LightComponentBase):
            if isinstance(light,u.DirectionalLightComponent): light.set_editor_property('intensity',.04)
            elif isinstance(light,u.SkyLightComponent): light.set_editor_property('intensity',.02)
    assert levels.save_current_level()
    report.append({'map':name,'directors':list(directors),'story_items':[x[0] for x in specs],
                   'wood_doors':[a.get_actor_label() for a in actors.get_all_level_actors() if isinstance(a,u.HSWoodDoor)],
                   'flicker_lamps':sum(isinstance(a,u.HSFlickerLamp) for a in actors.get_all_level_actors())})
(out/'JamSceneBinding.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
u.log('REBORN_JAM_STORY_CONFIGURED '+json.dumps(report))
