"""Remove tutorial signs from the three demo maps without rebuilding gameplay actors."""
import unreal as u
levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
for letter in "ABC":
    path="/HorrorSystems/Maps/Test_"+letter
    assert levels.load_level(path),path
    for actor in actors.get_all_level_actors():
        if isinstance(actor,u.TextRenderActor) and actor.get_actor_label() in ("WelcomeSign","PortalSign"):
            actors.destroy_actor(actor)
        elif isinstance(actor,(u.HSPickup,u.HSPortal)):
            actor.label.set_visibility(False)
    assert levels.save_current_level(),path
descriptions={
    "DA_Note":"灯熄灭以后，不要奔跑。它在远处追得很快，靠近时会放慢。钥匙留在走廊的尽头。穿过蓝色的门，记住你来时的方向。",
    "DA_Key":"一把生锈的钥匙，齿痕已经磨损。",
    "DA_Battery":"外壳冰冷的电池，触点上有暗红色的锈迹。",
    "DA_Token":"背面刻着一个模糊的名字。"
}
for name,description in descriptions.items():
    item=u.EditorAssetLibrary.load_asset("/HorrorSystems/Items/"+name)
    assert item,name
    item.set_editor_property("description",description)
    u.EditorAssetLibrary.save_loaded_asset(item)
u.log("HS_HINT_CLEANUP_SUCCESS")
