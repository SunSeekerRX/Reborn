"""Add title, third-stage inspection collapses and unlocked safety checkpoints."""
import unreal as u,json,shutil
from pathlib import Path
root='/HorrorSystems';eal=u.EditorAssetLibrary
levels=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
saved=Path(u.Paths.project_saved_dir(),'Verification');backup=saved/'BeforeTitleRecovery';backup.mkdir(parents=True,exist_ok=True)
u.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([root],force_rescan=True)
def spawn(cls,pos,label):
    a=actors.spawn_actor_from_class(cls,u.Vector(*pos));assert a,label;a.set_actor_label(label);return a
source=Path(__file__).resolve().parent/'ArtSources/MobiusPixel.png'
task=u.AssetImportTask();task.filename=str(source);task.destination_path=root+'/UI';task.destination_name='T_MobiusPixel';task.automated=True;task.replace_existing=True;task.save=True
u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
texture=eal.load_asset(root+'/UI/T_MobiusPixel');assert texture
texture.set_editor_property('filter',u.TextureFilter.TF_NEAREST);texture.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_EDITOR_ICON)
texture.set_editor_property('mip_gen_settings',u.TextureMipGenSettings.TMGS_NO_MIPMAPS);eal.save_loaded_asset(texture)
title=root+'/Maps/RebornTitle'
if eal.does_asset_exist(title):assert levels.load_level(title)
else:assert levels.new_level(title)
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world();world.get_world_settings().set_editor_property('default_game_mode',u.HSTitleGameMode)
assert levels.save_current_level()
report=[]
record_path=root+'/Items/DA_CollapseRecord'
record=eal.load_asset(record_path) if eal.does_asset_exist(record_path) else eal.duplicate_asset(root+'/Items/DA_FinalInformation',record_path)
assert record,'Collapse record asset could not be loaded'
record.set_editor_property('item_id','CollapseRecord');record.set_editor_property('display_name','家具旁的记录')
record.set_editor_property('description','这里的家具已经松动。合上记录后，它会向房间内侧倒下。道路旁还有可以通过的空间。')
assert eal.save_loaded_asset(record)
for name in ('Basic_roomA','Basic_roomB','Basic_roomC','Basic_roomABC'):
    assert levels.load_level(root+'/Maps/'+name)
    world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world();actual=world.get_name()
    file=Path(u.Paths.project_dir(),'Plugins/HorrorSystems/Content/Maps',actual+'.umap')
    if not (backup/file.name).exists():shutil.copy2(file,backup/file.name)
    existing=list(actors.get_all_level_actors())
    if name!='Basic_roomABC':
        for a in existing:
            if isinstance(a,u.HSCollapsingObstacle):a.minimum_collapse_stage=3
        assert levels.save_current_level();report.append({'map':name,'collapse_enabled':False});continue
    shelves=[a for a in existing if 'bookshelf' in a.get_actor_label().lower()]
    for n,old in enumerate(shelves):
        label=old.get_actor_label();component=old.get_component_by_class(u.StaticMeshComponent);assert component
        center,extent=old.get_actor_bounds(False)
        if isinstance(old,u.HSCollapsingObstacle):obstacle=old
        else:
            obstacle=spawn(u.HSCollapsingObstacle,(0,0,0),label)
            obstacle.set_actor_transform(component.get_world_transform(),False,False);obstacle.mesh.set_static_mesh(component.static_mesh)
            for i in range(component.get_num_materials()):obstacle.mesh.set_material(i,component.get_material(i))
            actors.destroy_actor(old)
        direction=-1 if center.x>-500 else 1
        pivot=u.Vector(center.x+direction*extent.x,center.y,center.z-extent.z)
        obstacle.fall_pivot_offset=obstacle.get_actor_transform().inverse_transform_location(pivot)
        obstacle.fall_axis=u.Vector(0,direction,0);obstacle.fall_angle=90;obstacle.fall_duration=1.2;obstacle.minimum_collapse_stage=3
        trigger_label='Reborn_CollapseRecord_'+str(n+1)
        triggers=[a for a in actors.get_all_level_actors() if isinstance(a,u.HSInspectTrigger) and a.get_actor_label()==trigger_label]
        pos=(center.x+direction*140,center.y-extent.y-90,220)
        trigger=triggers[0] if triggers else spawn(u.HSInspectTrigger,pos,trigger_label)
        trigger.item_data=record;trigger.obstacle=obstacle;trigger.mesh.set_static_mesh(record.inspection_mesh)
        trigger.mesh.set_relative_scale3d(u.Vector(.28,.28,.12))
        report.append({'shelf':label,'direction':direction,'trigger':trigger_label})
    for tag,pos in [('Safe_B',(-200,-1550,300)),('Safe_C',(-200,-5547,298)),('Safe_Final',(-200,-8700,299))]:
        label='Reborn_Recovery_'+tag
        found=[a for a in actors.get_all_level_actors() if isinstance(a,u.HSRecoveryCheckpoint) and a.get_actor_label()==label]
        checkpoint=found[0] if found else spawn(u.HSRecoveryCheckpoint,pos,label)
        checkpoint.spawn_transform=u.Transform(location=u.Vector(pos[0],pos[1],pos[2]-8),rotation=u.Rotator(pitch=0,yaw=-90,roll=0))
        report.append({'checkpoint':tag,'position':pos,'requires':'Key_3'})
    assert levels.save_current_level()
(saved/'TitleRecoveryConfig.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
u.log('TITLE_RECOVERY_CONFIGURED')
