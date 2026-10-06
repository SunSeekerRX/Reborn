"""Bind authored Basic maps to gameplay. Run with the UE Python commandlet.

Original maps are backed up once; furniture mesh/material/transform are preserved.
"""
import unreal as u
from pathlib import Path
import json
import shutil
import sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
from BasicGeometry import cut_basic_room_openings
from FirstRoomSetup import configure_first_room_references

root='/HorrorSystems'
eal=u.EditorAssetLibrary
assets=u.AssetToolsHelpers.get_asset_tools()
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
mel=u.MaterialEditingLibrary
project=Path(u.Paths.project_dir())
backup=Path(u.Paths.project_saved_dir(),'Verification','BasicBeforeAdaptation')
backup.mkdir(parents=True,exist_ok=True)
names=['Basic_roomA','Basic_roomB','Basic_roomC','Basic_roomABC']
for name in names:
    source=project/'Plugins/HorrorSystems/Content/Maps'/(name+'.umap')
    if not (backup/source.name).exists(): shutil.copy2(source,backup/source.name)
u.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([root],force_rescan=True)

def spawn(cls,pos,label):
    a=actors.spawn_actor_from_class(cls,u.Vector(*pos)); assert a,label
    a.set_actor_label(label); return a

def data(name,cls,folder):
    path=root+'/'+folder+'/'+name
    if eal.does_asset_exist(path): return eal.load_asset(path)
    f=u.DataAssetFactory(); f.set_editor_property('data_asset_class',cls)
    return assets.create_asset(name,root+'/'+folder,cls,f)

def overlay(name,color):
    path=root+'/Materials/'+name
    mat=eal.load_asset(path) if eal.does_asset_exist(path) else assets.create_asset(name,root+'/Materials',u.Material,u.MaterialFactoryNew())
    mat.set_editor_property('blend_mode',u.BlendMode.BLEND_ADDITIVE)
    mat.set_editor_property('shading_model',u.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('two_sided',True)
    mel.delete_all_material_expressions(mat)
    c=mel.create_material_expression(mat,u.MaterialExpressionConstant3Vector,0,0)
    c.constant=u.LinearColor(*color,1)
    mel.connect_material_property(c,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
    alpha=mel.create_material_expression(mat,u.MaterialExpressionConstant,0,160); alpha.r=.35
    mel.connect_material_property(alpha,'',u.MaterialProperty.MP_OPACITY)
    mel.recompile_material(mat); assert eal.save_loaded_asset(mat)
    return mat

overlay('M_HighlightPickup',(2.0,1.1,.10))
overlay('M_HighlightPainting',(.05,1.2,2.6))
rules=[]
for i,c in enumerate('ABC'):
    rule=data('DA_Room'+c,u.HSRoomRules,'Rooms')
    rule.set_editor_property('room_id','Room'+c)
    rule.set_editor_property('room_name',['第一房间','第二房间','第三房间'][i]); rule.set_editor_property('duration',60)
    routes=[]
    for stage in (1,2,3):
        route=u.HSRoomRoute(); route.stage=stage; route.target_room='Room'+'ABC'[(i+1)%3]
        route.set_editor_property('destination',eal.load_asset(root+'/Maps/Basic_room'+'ABC'[(i+1)%3]))
        route.required_clues=['Key_'+str(stage)]; route.advance_stage=i==2; route.finish_at_final_stage=i==2
        routes.append(route)
    rule.set_editor_property('routes',routes); rule.set_editor_property('clue_labels',{'Key_1':'旧钥匙','Key_2':'封存钥匙','Key_3':'终章钥匙'})
    assert eal.save_loaded_asset(rule); rules.append(rule)
note=eal.load_asset(root+'/Items/DA_Note'); key=eal.load_asset(root+'/Items/DA_Key')
trigger_item=data('DA_BookshelfNote',u.HSItemData,'Items')
trigger_item.set_editor_property('item_id','BookshelfNote'); trigger_item.set_editor_property('display_name','书柜旁的记录')
trigger_item.set_editor_property('description','书柜的底座似乎已经松动。记录背面有一道新鲜的划痕。')
trigger_item.set_editor_property('inspection_mesh',note.inspection_mesh)
trigger_item.set_editor_property('inspection_materials',note.inspection_materials)
trigger_item.set_editor_property('inspection_scale',note.inspection_scale)
trigger_item.set_editor_property('inspection_rotation',note.inspection_rotation)
trigger_item.set_editor_property('inspection_image',note.inspection_image)
assert eal.save_loaded_asset(trigger_item)

def model(component,item):
    component.set_static_mesh(item.inspection_mesh)
    component.set_relative_scale3d(u.Vector(1,1,1))
    for n,m in enumerate(item.inspection_materials): component.set_material(n,m)
    component.set_collision_profile_name('BlockAllDynamic')

def convert(old,cls,label):
    a=spawn(cls,(0,0,0),label)
    a.set_actor_transform(old.get_actor_transform(),False,False)
    source=old.get_component_by_class(u.StaticMeshComponent)
    a.mesh.set_static_mesh(source.static_mesh)
    for i in range(source.get_num_materials()): a.mesh.set_material(i,source.get_material(i))
    actors.destroy_actor(old)
    return a

def cut_openings(shell,name,safe_y,window_y,z):
    # Copy rather than edit the user's generated mesh shared by multiple scenes.
    path=root+'/Rooms/SM_'+name+'_GameplayShell'
    existing=eal.load_asset(path) if eal.does_asset_exist(path) else None
    if existing:
        # Existing authored copy can be incrementally opened without touching its source.
        mesh=existing
    else:
        mesh=assets.duplicate_asset('SM_'+name+'_GameplayShell',root+'/Rooms',shell.static_mesh_component.static_mesh)
    source_path=eal.get_metadata_tag(mesh,'OriginalShellMesh')
    source_mesh=eal.load_asset(source_path) if source_path else mesh
    cut_basic_room_openings(source_mesh,mesh,shell.get_actor_transform(),safe_y,window_y,z)
    shell.static_mesh_component.set_static_mesh(mesh)
    shell.static_mesh_component.set_collision_profile_name('BlockAll')

report=[]
for name in names:
    assert levels.load_level(root+'/Maps/'+name),name
    combined=name=='Basic_roomABC'
    all_actors=list(actors.get_all_level_actors())
    configure_first_room_references(name, all_actors)
    # Idempotent reruns retain converted furniture and remove only generated auxiliaries.
    for a in all_actors:
        if a.get_actor_label().startswith('Reborn_') and not isinstance(a,(u.HSCollapsingObstacle,u.HSSwapPainting,u.HSMovableProp,u.HSSceneAudio)):
            actors.destroy_actor(a)
        elif isinstance(a,u.SkeletalMeshActor):
            a.get_component_by_class(u.SkeletalMeshComponent).set_collision_response_to_channel(u.CollisionChannel.ECC_PAWN,u.CollisionResponseType.ECR_IGNORE)
    all_actors=list(actors.get_all_level_actors())
    starts=[a for a in all_actors if isinstance(a,u.PlayerStart)]
    for extra in starts[1:]: actors.destroy_actor(extra)
    initial=starts[0] if starts else spawn(u.PlayerStart,(-200,-1020,292),'PlayerStart')
    initial.player_start_tag='Safe_A' if combined else 'Safe_'+name[-1]
    initial.set_actor_location(u.Vector(-200,-1020,292),False,False)
    initial.set_actor_rotation(u.Rotator(0,90,0),False)
    scope=range(3) if combined else ['ABC'.index(name[-1])]
    rooms=[]
    for i in scope:
        c='ABC'[i]
        offset=0 if not combined or i==0 else (-2800 if i==1 else -6797)
        sign=1 if not combined or i==0 else -1
        floor_z=98 if combined and i==2 else 100
        center_y=200+offset
        safe_y=-850 if sign==1 else center_y+1050
        spawn_y=-1020 if sign==1 else center_y+1220
        exit_y=-550 if sign==1 else center_y+750
        window_y=850 if sign==1 else center_y-650
        if combined and i>0:
            safe=spawn(u.PlayerStart,(-200,spawn_y,floor_z+192),'Reborn_SafeStart_'+c)
            safe.player_start_tag='Safe_'+c; safe.set_actor_rotation(u.Rotator(0,90*sign,0),False)
        shell=min([a for a in all_actors if isinstance(a,u.StaticMeshActor) and a.get_actor_label().startswith('CubeGrid')],key=lambda a:abs(a.get_actor_bounds(False)[0].y-center_y))
        cut_openings(shell,name+'_'+c,exit_y,window_y,floor_z)
        director=spawn(u.HSRoomDirector,(-200,safe_y,floor_z+200),'Reborn_SafeRoom_'+c)
        director.rules=rules[i]; director.safe_spawn_tag='Safe_'+c
        director.safe_area.set_box_extent(u.Vector(200,250,200),False)
        director.return_barrier.set_box_extent(u.Vector(200,250,200),False)
        director.use_room_bounds=combined
        director.room_bounds.set_relative_location(u.Vector(-350,center_y-safe_y,50),False,False)
        director.room_bounds.set_box_extent(u.Vector(650,1399,500),False)
        window=spawn(u.HSWindowSequence,(0,window_y,0),'Reborn_Window_'+c)
        window.camera_offset=u.Vector(-70,0,floor_z+170); window.window_view_rotation=u.Rotator(0,0,0)
        if name=='Basic_roomA': window.camera_waypoints=[u.Vector(-220,-250,300),u.Vector(-220,-100,300)]
        window.monster_start=u.Vector(170,-450,floor_z+92); window.monster_look_point=u.Vector(170,0,floor_z+92)
        window.monster_end=u.Vector(170,900,floor_z+92)
        director.window_sequence=window
        outside=spawn(u.PointLight,(180,window_y,floor_z+280),'Reborn_WindowLight_'+c)
        outside.point_light_component.set_mobility(u.ComponentMobility.MOVABLE)
        outside.point_light_component.set_intensity(2500); outside.point_light_component.set_attenuation_radius(700)
        nav=spawn(u.NavMeshBoundsVolume,(-550,center_y,450),'Reborn_Nav_'+c)
        nav.set_actor_scale3d(u.Vector(8,16,6))
        monster=spawn(u.HSMonster,(-800,center_y+sign*900,floor_z+100),'Reborn_Monster_'+c)
        monster.home_room='Room'+c; monster.can_damage_player=True; monster.automatic_speed=True
        monster.stagger_duration=5; monster.stagger_speed=30
        portal=spawn(u.HSPortal,(-200,center_y+sign*1250,floor_z+150),'Reborn_Exit_'+c)
        portal.set_actor_rotation(u.Rotator(0,90,0),False)
        portal.use_stage_route=True; portal.room_rules=rules[i]; portal.destination_spawn_tag='Safe_'+'ABC'[(i+1)%3]
        portal.travel_enabled=name!='Basic_roomA'
        portal.local_travel=combined; portal.local_target_room='Room'+'ABC'[(i+1)%3]
        portal.set_editor_property('destination',eal.load_asset(root+'/Maps/Basic_room'+'ABC'[(i+1)%3]))
        portal.marker.set_material(0,eal.load_asset(root+'/Materials/M_Portal'))
        for stage in (1,2,3):
            p=spawn(u.HSPickup,(-700,center_y-sign*350,floor_z+120),'Reborn_Key_'+c+'_'+str(stage))
            p.item_data=key; p.pickup_id=c+'_Stage_'+str(stage); p.clue_id='Key_'+str(stage)
            p.minimum_stage=stage; p.maximum_stage=stage; p.rotate_for_demo=False; model(p.mesh,key)
        if i==0:
            p=spawn(u.HSPickup,(-450,center_y-sign*150,floor_z+115),'Reborn_Note_'+c)
            p.item_data=note; p.pickup_id=c+'_Note'; p.rotate_for_demo=False; model(p.mesh,note)
        furniture=[a for a in all_actors if isinstance(a,(u.StaticMeshActor,u.HSCollapsingObstacle,u.HSSwapPainting,u.HSMovableProp)) and abs(a.get_actor_bounds(False)[0].y-center_y)<1400 and not a.get_actor_label().startswith('CubeGrid')]
        drawers=[a for a in furniture if isinstance(a,u.HSMovableProp) or 'cabinet' in a.get_actor_label().lower()]
        if drawers:
            old=drawers[0]
            drawer=old if isinstance(old,u.HSMovableProp) else convert(old,u.HSMovableProp,'Reborn_Drawer_'+c)
            drawer.open_offset=u.Vector(0,0,80); drawer.open_rotation=u.Rotator(0,0,0)
        books=[a for a in furniture if 'Bookshelf' in a.get_actor_label()]
        assert len(books)==1,(name,c,'books',len(books))
        book=books[0] if isinstance(books[0],u.HSCollapsingObstacle) else convert(books[0],u.HSCollapsingObstacle,'Reborn_Bookshelf_'+c)
        book_center,book_extent=book.get_actor_bounds(False)
        pivot=u.Vector(book_center.x+book_extent.x,book_center.y,book_center.z-book_extent.z)
        book.fall_pivot_offset=book.get_actor_transform().inverse_transform_location(pivot)
        book.fall_axis=u.Vector(0,1,0); book.fall_angle=90; book.fall_duration=1.2
        trigger=spawn(u.HSInspectTrigger,(book_center.x+110,book_center.y+book_extent.y+70,floor_z+120),'Reborn_BookshelfRecord_'+c)
        trigger.item_data=trigger_item; trigger.obstacle=book; model(trigger.mesh,trigger_item)
        pictures=[a for a in furniture if 'Drawing' in a.get_actor_label() or isinstance(a,u.HSSwapPainting)]
        assert len(pictures)==3,(name,c,'paintings',len(pictures))
        for n,old in enumerate(sorted(pictures,key=lambda a:a.get_actor_location().y)):
            pic=old if isinstance(old,u.HSSwapPainting) else convert(old,u.HSSwapPainting,'Reborn_Painting_'+c+'_'+str(n+1))
            pic.swap_group='Room'+c; pic.swap_duration=.9
        rooms.append({'room':c,'safe_y':safe_y,'window_y':window_y,'bookshelf':[book_center.x,book_center.y,book_center.z],'paintings':3})
    spawn(u.HSVisionRig,(0,0,0),'Reborn_Vision')
    world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property('default_game_mode',u.HSGameMode)
    assert levels.save_current_level(),name
    report.append({'map':name,'rooms':rooms})
Path(u.Paths.project_saved_dir(),'Verification','BasicGameplayConfig.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
u.log('REBORN_BASIC_GAMEPLAY_CONFIGURED')
