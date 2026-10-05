"""Upgrade existing whitebox maps with stage routes, entry vestibules and window performances."""
import unreal as u
from pathlib import Path
import json

eal=u.EditorAssetLibrary
assets=u.AssetToolsHelpers.get_asset_tools()
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
root="/HorrorSystems"
u.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([root],force_rescan=True)
maps=[root+"/Maps/Test_"+c for c in "ABC"]
cube=eal.load_asset("/Engine/BasicShapes/Cube")
wall=eal.load_asset(root+"/Materials/M_Wall")
floor=eal.load_asset(root+"/Materials/M_Floor")
portal_mat=eal.load_asset(root+"/Materials/M_Portal")
pickup_mat=eal.load_asset(root+"/Materials/M_Pickup")
def spawn(cls,pos,label):
    a=actors.spawn_actor_from_class(cls,u.Vector(*pos)); assert a,label
    a.set_actor_label(label); return a
def block(pos,scale,label,mat=wall):
    a=spawn(u.StaticMeshActor,pos,label); a.static_mesh_component.set_static_mesh(cube)
    a.static_mesh_component.set_material(0,mat); a.static_mesh_component.set_collision_profile_name("BlockAll")
    a.set_actor_scale3d(u.Vector(*scale)); return a

source=Path(__file__).parent/"MediaSource"
tasks=[]
for name in ("S_MonsterStep","S_MonsterPresence","S_Prop"):
    t=u.AssetImportTask(); t.filename=str(source/(name+".wav")); t.destination_path=root+"/Audio"
    t.automated=True; t.replace_existing=True; t.save=True; tasks.append(t)
assets.import_asset_tasks(tasks)
presence=eal.load_asset(root+"/Audio/S_MonsterPresence"); presence.set_editor_property("looping",True); eal.save_loaded_asset(presence)

rules=[]
for i,letter in enumerate("ABC"):
    path=root+"/Rooms/DA_Room"+letter
    factory=u.DataAssetFactory(); factory.set_editor_property("data_asset_class",u.HSRoomRules)
    data=eal.load_asset(path) if eal.does_asset_exist(path) else assets.create_asset("DA_Room"+letter,root+"/Rooms",u.HSRoomRules,factory)
    data.set_editor_property("room_id","Room"+letter); data.set_editor_property("room_name",["第一房间","第二房间","第三房间"][i]); data.set_editor_property("duration",60)
    routes=[]
    for stage in (1,2,3):
        r=u.HSRoomRoute(); r.stage=stage; r.target_room="Room"+"ABC"[(i+1)%3]
        r.set_editor_property("destination",eal.load_asset(maps[(i+1)%3]))
        r.required_clues=["Key_"+str(stage)]; r.advance_stage=i==2; r.finish_at_final_stage=i==2
        routes.append(r)
    data.set_editor_property("routes",routes)
    data.set_editor_property("clue_labels",{"Key_1":"旧钥匙","Key_2":"封存钥匙","Key_3":"终章钥匙"})
    eal.save_loaded_asset(data); rules.append(data)

key=eal.load_asset(root+"/Items/DA_Key")
note=eal.load_asset(root+"/Items/DA_Note")
note.set_editor_property("inspect_on_pickup",True); eal.save_loaded_asset(note)
report=[]
for i,path in enumerate(maps):
    assert levels.load_level(path)
    for a in actors.get_all_level_actors():
        label=a.get_actor_label()
        if isinstance(a,(u.HSPortal,u.HSPickup,u.HSRoomDirector,u.HSWindowSequence,u.HSMovableProp)) or label.startswith(("SafeWall_","Window_","Outdoor_")) or label in ("SouthWall","SafeStart"):
            actors.destroy_actor(a)
        elif isinstance(a,u.PlayerStart):
            a.set_actor_location(u.Vector(-1800,0,110),False,False)
    safe=spawn(u.PlayerStart,(-1800,0,110),"SafeStart"); safe.player_start_tag="Safe"
    # Small entry vestibule with a single opening facing the gameplay area.
    block((-2240,0,160),(.25,9.4,3.6),"SafeWall_Back")
    for side in (-1,1):
        block((-1790,side*470,160),(9,.25,3.6),"SafeWall_Side"+str(side))
        block((-1350,side*300,160),(.25,3.8,3.6),"SafeWall_Front"+str(side))
    # Opening in the south wall, with opaque sill/lintel and an exterior walkway.
    block((-1850,-1800,160),(15,1,4.3),"Window_WallLeft")
    block((950,-1800,160),(33,1,4.3),"Window_WallRight")
    block((-900,-1800,35),(4,1,1.5),"Window_Sill")
    block((-900,-1800,340),(4,1,.7),"Window_Lintel")
    block((-700,-2100,-55),(38,7,1),"Outdoor_Floor",floor)
    window=spawn(u.HSWindowSequence,(-900,-1800,0),"Window_Performance")
    window.camera_offset=u.Vector(0,350,175)
    director=spawn(u.HSRoomDirector,(-1800,0,140),"RoomDirector")
    director.rules=rules[i]; director.window_sequence=window
    door=spawn(u.HSPortal,(2150,-700,150),"Portal_0")
    door.set_editor_property("destination",eal.load_asset(maps[(i+1)%3])); door.destination_spawn_tag="Safe"
    door.use_stage_route=True; door.room_rules=rules[i]; door.marker.set_material(0,portal_mat)
    for stage in (1,2,3):
        p=spawn(u.HSPickup,(-950,-270,75),"Clue_"+"ABC"[i]+"_"+str(stage))
        p.item_data=key; p.pickup_id="Stage_"+str(stage)+"_Key"; p.clue_id="Key_"+str(stage)
        p.minimum_stage=stage; p.maximum_stage=stage; p.mesh.set_material(0,pickup_mat)
    if i==0:
        p=spawn(u.HSPickup,(-1150,0,75),"Pickup_A_0"); p.item_data=note; p.pickup_id="Demo_A_0"
        p.minimum_stage=1; p.maximum_stage=3; p.mesh.set_material(0,pickup_mat)
    prop=spawn(u.HSMovableProp,(-1000,250,65),"Movable_Drawer")
    prop.set_actor_scale3d(u.Vector(1,.8,.8)); prop.open_offset=u.Vector(100,0,0)
    prop.mesh.set_material(0,wall); prop.interaction_sound=eal.load_asset(root+"/Audio/S_Prop")
    # The window and doorway remain visible during the directed shot.
    light=spawn(u.PointLight,(-900,-1650,280),"Window_FillLight"); light.point_light_component.set_mobility(u.ComponentMobility.MOVABLE)
    light.point_light_component.set_intensity(400); light.point_light_component.set_attenuation_radius(1100)
    outside=spawn(u.PointLight,(-900,-2100,260),"Outdoor_PerformanceLight")
    outside.point_light_component.set_mobility(u.ComponentMobility.MOVABLE)
    outside.point_light_component.set_intensity(4000); outside.point_light_component.set_attenuation_radius(1000)
    assert levels.save_current_level(),path
    report.append({"map":path,"room":str(rules[i].room_id),"stages":3,"seconds":60,"actors":len(actors.get_all_level_actors())})
eal.save_directory(root,only_if_is_dirty=False,recursive=True)
levels.load_level(maps[0])
out=Path(u.Paths.project_saved_dir())/"Verification/RoomContent.json"; out.parent.mkdir(parents=True,exist_ok=True)
out.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
u.log("HS_ROOM_CONTENT_SUCCESS")
