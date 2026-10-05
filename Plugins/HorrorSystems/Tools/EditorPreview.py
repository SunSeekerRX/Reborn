"""Set a useful editor overview after opening the test project."""
import unreal as u
editor=u.get_editor_subsystem(u.LevelEditorSubsystem)
editor.set_level_viewport_camera_info(u.Vector(-2400,-1100,850),u.Rotator(-18,25,0),"")
u.log("Reborn project ready. Press Play to test systems. Content is in /HorrorSystems.")
