"""Run with UE 5.8 Editor Python commandlet. Generates portable plugin assets and three maps."""
import unreal as u
from pathlib import Path
import json

PLUGIN="/HorrorSystems"
PROJECT=Path(u.Paths.project_dir()).resolve()
SOURCE=PROJECT/"Plugins/HorrorSystems/Tools/MediaSource"
REPORT=PROJECT/"Saved/Verification/ContentBuild.json"
assets=u.AssetToolsHelpers.get_asset_tools()
eal=u.EditorAssetLibrary
u.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([PLUGIN,"/Game/Characters"],force_rescan=True)
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
mel=u.MaterialEditingLibrary

def must(value,message):
    if not value: raise RuntimeError(message)
    return value

# Relocate UE mannequin assets with Unreal's dependency-aware asset rename API.
# Fixing references here makes the plugin independent of /Game/Characters.
if eal.does_directory_exist("/Game/Characters"):
    paths=eal.list_assets("/Game/Characters",recursive=True,include_folder=False)
    renames=[]
    for path in paths:
        asset=eal.load_asset(path)
        if not asset or asset.get_class().get_name()=="ObjectRedirector": continue
        if asset.get_path_name().startswith(PLUGIN+"/"): continue
        package=path.split(".")[0]
        folder,name=package.rsplit("/",1)
        target=folder.replace("/Game/Characters",PLUGIN+"/Characters",1)
        if not eal.does_asset_exist(target+"/"+name): renames.append(u.AssetRenameData(asset,target,name))
    if renames: must(assets.rename_assets(renames),"Mannequin relocation failed")
    eal.save_directory(PLUGIN+"/Characters",only_if_is_dirty=False,recursive=True)

def import_media():
    tasks=[]
    for path in SOURCE.iterdir():
        if path.suffix.lower() not in (".png",".wav"): continue
        dest=PLUGIN+("/UI" if path.suffix==".png" else "/Audio")
        if eal.does_asset_exist(dest+"/"+path.stem): continue
        task=u.AssetImportTask(); task.filename=str(path); task.destination_path=PLUGIN+("/UI" if path.suffix==".png" else "/Audio")
        task.automated=True; task.replace_existing=True; task.save=True; tasks.append(task)
    assets.import_asset_tasks(tasks)
    ambience=eal.load_asset(PLUGIN+"/Audio/S_Ambience")
    ambience.set_editor_property("looping",True)
    eal.save_loaded_asset(ambience)
import_media()

def material(name,color,emissive=0):
    path=PLUGIN+"/Materials/"+name
    mat=eal.load_asset(path) if eal.does_asset_exist(path) else assets.create_asset(name,PLUGIN+"/Materials",u.Material,u.MaterialFactoryNew())
    mel.delete_all_material_expressions(mat)
    c=mel.create_material_expression(mat,u.MaterialExpressionConstant3Vector,0,0)
    c.constant=u.LinearColor(*color,1)
    mel.connect_material_property(c,"",u.MaterialProperty.MP_BASE_COLOR)
    if emissive:
        m=mel.create_material_expression(mat,u.MaterialExpressionMultiply,200,100); m.set_editor_property("const_b",emissive)
        mel.connect_material_expressions(c,"",m,"A"); mel.connect_material_property(m,"",u.MaterialProperty.MP_EMISSIVE_COLOR)
    rough=mel.create_material_expression(mat,u.MaterialExpressionConstant,0,200); rough.r=.8
    mel.connect_material_property(rough,"",u.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat); eal.save_loaded_asset(mat); return mat

m_floor=material("M_Floor",(.065,.08,.09))
m_wall=material("M_Wall",(.11,.13,.14))
m_pickup=material("M_Pickup",(.38,.3,.12),.4)
m_portal=material("M_Portal",(.08,.38,.48),2)
m_monster=material("M_Monster",(.14,.025,.032),.1)
mel.set_base_material_usage(m_monster,u.MaterialUsage.MATUSAGE_SKELETAL_MESH)
mel.recompile_material(m_monster); eal.save_loaded_asset(m_monster)

def distance_material():
    name="M_DistanceFade"; path=PLUGIN+"/Materials/"+name
    mat=eal.load_asset(path) if eal.does_asset_exist(path) else assets.create_asset(name,PLUGIN+"/Materials",u.Material,u.MaterialFactoryNew())
    mat.set_editor_property("material_domain",u.MaterialDomain.MD_POST_PROCESS)
    mel.delete_all_material_expressions(mat)
    def node(cls,x,y): return mel.create_material_expression(mat,cls,x,y)
    scene=node(u.MaterialExpressionSceneTexture,0,0); scene.set_editor_property("scene_texture_id",u.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    depth=node(u.MaterialExpressionSceneDepth,0,200)
    clear=node(u.MaterialExpressionScalarParameter,0,350); clear.set_editor_property("parameter_name","ClearDistance"); clear.set_editor_property("default_value",500)
    far=node(u.MaterialExpressionScalarParameter,0,500); far.set_editor_property("parameter_name","HiddenDistance"); far.set_editor_property("default_value",1600)
    sub=node(u.MaterialExpressionSubtract,200,200); span=node(u.MaterialExpressionSubtract,200,450)
    div=node(u.MaterialExpressionDivide,400,200); clamp=node(u.MaterialExpressionClamp,600,200)
    inv=node(u.MaterialExpressionOneMinus,800,200); mul=node(u.MaterialExpressionMultiply,1000,0)
    for src,out,dst,inp in [(depth,"",sub,"A"),(clear,"",sub,"B"),(far,"",span,"A"),(clear,"",span,"B"),(sub,"",div,"A"),(span,"",div,"B"),(div,"",clamp,""),(clamp,"",inv,""),(scene,"",mul,"A"),(inv,"",mul,"B")]:
        must(mel.connect_material_expressions(src,out,dst,inp),"Material connection failed: "+inp)
    mel.connect_material_property(mul,"",u.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat); eal.save_loaded_asset(mat)
distance_material()

def item(name,title,description,icon,image=None):
    path=PLUGIN+"/Items/"+name
    factory=u.DataAssetFactory(); factory.set_editor_property("data_asset_class",u.HSItemData)
    data=eal.load_asset(path) if eal.does_asset_exist(path) else assets.create_asset(name,PLUGIN+"/Items",u.HSItemData,factory)
    data.set_editor_property("item_id",name); data.set_editor_property("display_name",title); data.set_editor_property("description",description)
    data.set_editor_property("icon",eal.load_asset(PLUGIN+"/UI/"+icon))
    data.set_editor_property("inspection_image",eal.load_asset(PLUGIN+"/UI/"+(image or icon)))
    data.set_editor_property("pickup_sound",eal.load_asset(PLUGIN+"/Audio/S_Pickup"))
    data.set_editor_property("inspect_sound",eal.load_asset(PLUGIN+"/Audio/S_Inspect"))
    eal.save_loaded_asset(data); return data
items=[
    item("DA_Note","值班纸条","灯熄灭以后，不要奔跑。它在远处追得很快，靠近时会放慢。钥匙留在走廊的尽头。穿过蓝色的门，记住你来时的方向。","T_NoteIcon","T_Note"),
    item("DA_Key","旧钥匙","一把生锈的钥匙，齿痕已经磨损。","T_Key"),
    item("DA_Battery","备用电池","外壳冰冷的电池，触点上有暗红色的锈迹。","T_Battery"),
    item("DA_Token","红色徽章","背面刻着一个模糊的名字。","T_Token")
]

cube=eal.load_asset("/Engine/BasicShapes/Cube")
def spawn(cls,loc,label,rotation=None):
    actor=must(actors.spawn_actor_from_class(cls,u.Vector(*loc),rotation or u.Rotator()),"Spawn failed: "+label)
    actor.set_actor_label(label); return actor
def block(loc,scale,label,mat=m_wall):
    a=spawn(u.StaticMeshActor,loc,label); c=a.static_mesh_component; c.set_static_mesh(cube)
    a.set_actor_scale3d(u.Vector(*scale)); c.set_material(0,mat); c.set_collision_profile_name("BlockAll")
    return a
def text(loc,content,label,size=34):
    a=spawn(u.TextRenderActor,loc,label,u.Rotator(0,180,0))
    c=a.text_render; c.set_text(content); c.set_world_size(size); c.set_text_render_color(u.Color(149,194,184,255))
    return a

map_paths=[PLUGIN+"/Maps/Test_"+letter for letter in "ABC"]
for empty_path in map_paths:
    if not eal.does_asset_exist(empty_path):
        must(levels.new_level(empty_path),"Cannot create placeholder "+empty_path)
        must(levels.save_current_level(),"Cannot save placeholder "+empty_path)
summary=[]
for index,path in enumerate(map_paths):
    if eal.does_asset_exist(path):
        must(levels.load_level(path),"Cannot load map "+path)
        for old_actor in actors.get_all_level_actors():
            if old_actor.get_class().get_name() not in ("WorldSettings","Brush"):
                actors.destroy_actor(old_actor)
    else:
        must(levels.new_level(path),"Cannot create map "+path)
    letter="ABC"[index]
    world=unreal_world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property("default_game_mode",u.HSGameMode)
    floor=block((0,0,-55),(52,36,1),"CollisionFloor",m_floor)
    block((0,-1800,160),(52,1,4.3),"SouthWall")
    block((0,1800,160),(52,1,4.3),"NorthWall")
    block((-2600,0,160),(1,36,4.3),"WestWall")
    block((2600,0,160),(1,36,4.3),"EastWall")
    # Navigable obstacle route; a low beam tests standing up under obstruction.
    block((400,430,120),(2,5,2.4),"PathfindingObstacle")
    block((1050,-350,60),(4,1,1.2),"LowBarrier")
    block((-500,650,130),(3,2,.3),"CrouchClearanceBeam")
    block((-500,535,60),(.3,.3,1.5),"BeamSupport1")
    block((-500,765,60),(.3,.3,1.5),"BeamSupport2")
    start=spawn(u.PlayerStart,(-1800,0,110),"DefaultStart"); start.player_start_tag="Default"
    back=spawn(u.PlayerStart,(-1600,-850,110),"ArrivalStart"); back.player_start_tag="Arrival"
    back.set_actor_rotation(u.Rotator(0,0,0),False)
    rig=spawn(u.HSVisionRig,(0,0,0),"HorrorVision_FogAndDepthFade")
    amb=spawn(u.HSAmbientZone,(-1000,0,60),"AmbientAudio"); amb.sound=eal.load_asset(PLUGIN+"/Audio/S_Ambience"); amb.audible_radius=4000
    # Modest ambient illumination keeps the mannequin and collision test readable.
    for n,loc in enumerate([(-1750,0,300),(200,0,300),(1800,0,300)]):
        light=spawn(u.PointLight,loc,"TestLight_"+str(n)); c=light.point_light_component
        c.set_mobility(u.ComponentMobility.MOVABLE)
        c.set_intensity(120); c.set_attenuation_radius(1100); c.set_light_color(u.LinearColor(.55,.7,.85))
    for n in range(12):
        x=-1400+(n%4)*320; y=-270+(n//4)*300
        a=spawn(u.HSPickup,(x,y,75),"Pickup_"+letter+"_"+str(n))
        a.item_data=items[n%4]; a.pickup_id="Demo_"+letter+"_"+str(n)
        a.mesh.set_material(0,m_pickup)
        # Keep labels small and independent of the tiny pickup's transform.
        a.label.set_absolute(False,False,True); a.label.set_world_scale3d(u.Vector(1,1,1)); a.label.set_world_size(18)
        a.label.set_relative_location(u.Vector(0,0,180),False,False)
    monster=spawn(u.HSMonster,(1550,900,110),"B_PursuingMonster")
    for n,offset in enumerate([-700,700]):
        destination=map_paths[(index+(1 if n==0 else -1))%3]
        p=spawn(u.HSPortal,(2150,offset,150),"Portal_"+str(n),u.Rotator(0,180,0))
        p.set_editor_property("destination",eal.load_asset(destination))
        p.destination_spawn_tag="Arrival"; p.portal_name="TO TEST "+destination[-1]
        p.marker.set_material(0,m_portal)
    # Nav bounds volume's engine default brush provides a unit cube.
    nav=spawn(u.NavMeshBoundsVolume,(0,0,150),"NavigationBounds")
    nav.set_actor_scale3d(u.Vector(28,20,5))
    must(levels.save_current_level(),"Failed to save "+path)
    summary.append({"map":path,"actors":len(actors.get_all_level_actors()),"pickups":12,"portals":2,"monster":1})

eal.save_directory(PLUGIN,only_if_is_dirty=False,recursive=True)
levels.load_level(map_paths[0])
REPORT.parent.mkdir(parents=True,exist_ok=True)
REPORT.write_text(json.dumps({"maps":summary,"plugin_assets":len(eal.list_assets(PLUGIN,True,False)),"status":"generated"},ensure_ascii=False,indent=2),encoding="utf-8")
u.log("HS_CONTENT_BUILD_SUCCESS "+str(REPORT))
