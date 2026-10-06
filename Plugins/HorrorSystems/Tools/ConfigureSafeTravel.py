"""Add safe-return travel without modifying any existing room/furniture mesh."""
import unreal as u,json,shutil
from pathlib import Path
root='/HorrorSystems';eal=u.EditorAssetLibrary
levels=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
assets=u.AssetToolsHelpers.get_asset_tools();saved=Path(u.Paths.project_saved_dir(),'Verification')
backup=saved/'BeforeThreeStageFlow';backup.mkdir(parents=True,exist_ok=True)
u.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([root],force_rescan=True)
map_paths={}
for name in ('Basic_roomA','Basic_roomB','Basic_roomC','Basic_roomABC'):
    assert levels.load_level(root+'/Maps/'+name)
    map_paths[name]=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world().get_path_name()
def spawn(cls,pos,label):
    a=actors.spawn_actor_from_class(cls,u.Vector(*pos));assert a,label
    a.set_actor_label(label);return a
def snapshot(all_actors):
    result={}
    for a in all_actors:
        if a.get_actor_label().startswith(('Reborn_SafePortal_','Reborn_Key_')):continue
        for c in a.get_components_by_class(u.StaticMeshComponent):
            if c.static_mesh:
                t=c.get_world_transform();p=t.translation;r=t.rotation;s=t.scale3d
                result[a.get_name()+':'+c.get_name()]={'mesh':c.static_mesh.get_path_name(),'transform':[round(x,6) for x in (p.x,p.y,p.z,r.x,r.y,r.z,r.w,s.x,s.y,s.z)],'materials':[c.get_material(i).get_path_name() if c.get_material(i) else '' for i in range(c.get_num_materials())]}
    return result
rules=[]
for i,c in enumerate('ABC'):
    path=root+'/Rooms/DA_SafeTravelRoom'+c
    if eal.does_asset_exist(path):rule=eal.load_asset(path)
    else:
        factory=u.DataAssetFactory();factory.set_editor_property('data_asset_class',u.HSRoomRules)
        rule=assets.create_asset('DA_SafeTravelRoom'+c,root+'/Rooms',u.HSRoomRules,factory)
    rule.set_editor_property('room_id','Room'+c);rule.set_editor_property('room_name',c+' 房间');rule.set_editor_property('duration',60)
    routes=[]
    for stage in (1,2,3):
        r=u.HSRoomRoute();r.stage=stage;r.required_clues=['Key_'+str(stage)]
        target=('B','C','C')[i] if stage==1 else ('A','A','B')[i]
        r.target_room='Room'+('A' if stage==3 or (stage==2 and i==0) else target)
        map_name='Basic_roomABC' if stage==3 or (i==0 and stage==2) else 'Basic_room'+target
        r.set_editor_property('destination',eal.load_asset(map_paths[map_name]))
        r.advance_stage=False;r.advance_on_safe_return=(stage==1 and i==1) or (stage==2 and i==0)
        r.finish_at_final_stage=stage==3;routes.append(r)
    rule.set_editor_property('routes',routes);rule.set_editor_property('clue_labels',{'Key_1':'关键物品','Key_2':'第二关关键物品','Key_3':'第三关关键物品'})
    assert eal.save_loaded_asset(rule);rules.append(rule)
mat_path=root+'/Materials/M_SafeWhitePortal'
material=eal.load_asset(mat_path) if eal.does_asset_exist(mat_path) else assets.create_asset('M_SafeWhitePortal',root+'/Materials',u.Material,u.MaterialFactoryNew())
material.set_editor_property('shading_model',u.MaterialShadingModel.MSM_UNLIT);material.set_editor_property('two_sided',True)
u.MaterialEditingLibrary.delete_all_material_expressions(material)
color=u.MaterialEditingLibrary.create_material_expression(material,u.MaterialExpressionConstant3Vector,0,0);color.constant=u.LinearColor(30,30,30,1)
u.MaterialEditingLibrary.connect_material_property(color,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
u.MaterialEditingLibrary.recompile_material(material);assert eal.save_loaded_asset(material)
key=eal.load_asset(root+'/Items/DA_Key');assert key
final_path=root+'/Items/DA_FinalInformation'
final_item=eal.load_asset(final_path) if eal.does_asset_exist(final_path) else eal.duplicate_asset(root+'/Items/DA_Key',final_path)
final_item.set_editor_property('item_id','FinalInformation');final_item.set_editor_property('display_name','最终信息')
final_item.set_editor_property('description','最后的出口在这条相连走廊的另一端。穿过 B、C，继续向前跑。')
final_item.set_editor_property('inspect_on_pickup',False);assert eal.save_loaded_asset(final_item)
report=[]
for name in ('Basic_roomA','Basic_roomB','Basic_roomC','Basic_roomABC'):
    file=Path(u.Paths.project_dir(),'Plugins/HorrorSystems/Content/Maps',name+'.umap')
    if not (backup/file.name).exists():shutil.copy2(file,backup/file.name)
    assert levels.load_level(root+'/Maps/'+name)
    actual_name=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world().get_name()
    actual_file=Path(u.Paths.project_dir(),'Plugins/HorrorSystems/Content/Maps',actual_name+'.umap')
    if actual_file.exists() and not (backup/actual_file.name).exists():shutil.copy2(actual_file,backup/actual_file.name)
    existing=list(actors.get_all_level_actors());before=snapshot(existing)
    combined=name=='Basic_roomABC';scope=(0,) if combined else ['ABC'.index(name[-1])]
    if combined:
        for old in existing:
            if isinstance(old,u.HSRoomDirector) and old.safe_spawn_tag!='Safe_A':actors.destroy_actor(old)
            elif isinstance(old,u.HSPickup) and old.get_actor_label().startswith(('Reborn_Key_B_','Reborn_Key_C_')):actors.destroy_actor(old)
        existing=list(actors.get_all_level_actors())
        before=snapshot(existing)
    for i in scope:
        c='ABC'[i];offset=0 if not combined or i==0 else (-2800 if i==1 else -6797)
        sign=1 if not combined or i==0 else -1;floor=98 if combined and i==2 else 100;center=200+offset
        safe_y=-850 if sign==1 else center+1050;spawn_y=-1020 if sign==1 else center+1220
        directors=[a for a in actors.get_all_level_actors() if isinstance(a,u.HSRoomDirector) and a.safe_spawn_tag=='Safe_'+c]
        director=directors[0] if directors else spawn(u.HSRoomDirector,(-200,safe_y,floor+200),'Reborn_SafeRoom_'+c)
        director.rules=rules[i];director.safe_spawn_tag='Safe_'+c;director.return_to_safe_after_objective=not combined
        director.minimum_stage=3 if combined else 1;director.use_room_bounds=False;director.final_escape_mode=combined
        if not combined:director.play_first_entry_sequence=c=='B'
        director.safe_area.set_box_extent(u.Vector(200,250,200),False)
        director.room_bounds.set_relative_location(u.Vector(-350,center-safe_y,50),False,False);director.room_bounds.set_box_extent(u.Vector(650,1399,500),False)
        starts=[a for a in actors.get_all_level_actors() if isinstance(a,u.PlayerStart) and (a.player_start_tag=='Safe_'+c or (i==0 and not combined))]
        if not starts:starts=[a for a in actors.get_all_level_actors() if isinstance(a,u.PlayerStart)] if not combined else []
        start=starts[0] if starts else spawn(u.PlayerStart,(-200,spawn_y,floor+192),'Reborn_SafeStart_'+c)
        start.player_start_tag='Safe_'+c
        start.set_actor_rotation(u.Rotator(pitch=0,yaw=90*sign,roll=0),False)
        if not combined:
            for window in [a for a in actors.get_all_level_actors() if isinstance(a,u.HSWindowSequence)]:
                window.camera_waypoints=[u.Vector(-220,-250,300),u.Vector(-220,-100,300)]
            for old in [a for a in actors.get_all_level_actors() if isinstance(a,u.HSPortal) and not a.get_actor_label().startswith('Reborn_SafePortal_')]:old.travel_enabled=False
            doors=[a for a in actors.get_all_level_actors() if isinstance(a,u.HSPortal) and a.get_actor_label()=='Reborn_SafePortal_'+c]
            door=doors[0] if doors else spawn(u.HSPortal,(-200,-1060,floor+250),'Reborn_SafePortal_'+c)
            door.set_actor_rotation(u.Rotator(pitch=0,yaw=90,roll=0),False)
            door.trigger.set_box_extent(u.Vector(16,100,135),False)
            door.marker.set_relative_location(u.Vector(-20,0,0),False,False);door.marker.set_relative_scale3d(u.Vector(.08,2,2.6));door.marker.set_material(0,material)
            door.travel_enabled=True;door.use_stage_route=True;door.room_rules=rules[i];door.requires_safe_return=True;door.white_light_travel=True
            door.destination_spawn_tag='Safe_'+('B' if c=='A' else 'C' if c=='B' else 'B');door.local_travel=False
        for stage in ((3,) if combined else (1,2)):
            clue='Key_'+str(stage)
            pickups=[a for a in actors.get_all_level_actors() if isinstance(a,u.HSPickup) and a.clue_id==clue and (not combined or a.get_actor_label().startswith('Reborn_Key_'+c+'_'))]
            if not pickups:
                pickup=spawn(u.HSPickup,(-700,center+sign*100,floor+120),'Reborn_Key_'+c+'_'+str(stage))
                pickup.item_data=key;pickup.pickup_id=c+'_Stage_'+str(stage);pickup.clue_id=clue
                pickup.minimum_stage=stage;pickup.maximum_stage=stage;pickup.rotate_for_demo=False
                pickup.mesh.set_static_mesh(key.inspection_mesh)
                for n,m in enumerate(key.inspection_materials):pickup.mesh.set_material(n,m)
            if combined:
                for pickup in actors.get_all_level_actors():
                    if isinstance(pickup,u.HSPickup) and pickup.clue_id=='Key_3':pickup.item_data=final_item
            for pickup in actors.get_all_level_actors():
                if isinstance(pickup,u.HSPickup) and pickup.get_actor_label()=='Reborn_Key_'+c+'_'+str(stage):pickup.set_actor_location(u.Vector(-700,center+sign*100,floor+120),False,False)
        report.append({'map':name,'room':c,'safe_spawn':start.get_actor_label(),'return_objective':not combined,'stage_minimum':director.minimum_stage})
    if combined:
        gates=[a for a in actors.get_all_level_actors() if isinstance(a,u.HSPortal) and a.get_actor_label()=='Reborn_FinalExit']
        gate=gates[0] if gates else spawn(u.HSPortal,(-200,-8900,250),'Reborn_FinalExit')
        gate.set_actor_rotation(u.Rotator(pitch=0,yaw=90,roll=0),False)
        gate.trigger.set_box_extent(u.Vector(40,180,150),False)
        gate.marker.set_relative_scale3d(u.Vector(.08,3.6,3));gate.marker.set_material(0,material)
        gate.travel_enabled=True;gate.use_stage_route=True;gate.room_rules=rules[0]
        gate.requires_safe_return=False;gate.white_light_travel=True
    if not any(isinstance(a,u.HSSceneAudio) for a in existing):spawn(u.HSSceneAudio,(0,0,0),'Reborn_SceneAudio')
    after=snapshot(existing);assert before==after,(name,'An existing model changed')
    assert levels.save_current_level()
    report.append({'map':name,'existing_mesh_components_preserved':len(before)})
(saved/'SafeTravelMapConfig.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
u.log('SAFE_TRAVEL_MAPS_CONFIGURED')
