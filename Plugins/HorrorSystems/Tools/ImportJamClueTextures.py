"""Assign story-specific paper materials to saved pickups and inspection models."""
import os
from pathlib import Path
import unreal as u
folder='/HorrorSystems/Story'
eal=u.EditorAssetLibrary;tools=u.AssetToolsHelpers.get_asset_tools();mel=u.MaterialEditingLibrary
u.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(['/HorrorSystems'],force_rescan=True)
for file in Path(os.environ['REBORN_CLUE_TEXTURE_ROOT']).glob('T_*.png'):
 task=u.AssetImportTask();task.filename=str(file);task.destination_path=folder+'/Textures';task.destination_name=file.stem;task.automated=True;task.replace_existing=True;task.save=True
 tools.import_asset_tasks([task]);texture=eal.load_asset(task.destination_path+'/'+file.stem);assert texture
 name='M_'+file.stem[2:];path=folder+'/Materials/'+name
 mat=eal.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(name,folder+'/Materials',u.Material,u.MaterialFactoryNew())
 mat.set_editor_property('used_with_nanite',True)
 mel.delete_all_material_expressions(mat)
 sample=mel.create_material_expression(mat,u.MaterialExpressionTextureSample,0,0);sample.set_editor_property('texture',texture)
 mel.connect_material_property(sample,'RGB',u.MaterialProperty.MP_BASE_COLOR)
 rough=mel.create_material_expression(mat,u.MaterialExpressionConstant,0,180);rough.set_editor_property('r',.8)
 mel.connect_material_property(rough,'',u.MaterialProperty.MP_ROUGHNESS);mel.recompile_material(mat)
 item=eal.load_asset(folder+'/DA_'+file.stem[2:]);assert item
 item.set_editor_property('inspection_materials',[mat]);item.set_editor_property('inspection_image',texture)
 assert eal.save_loaded_asset(mat) and eal.save_loaded_asset(item)
levels=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
for name in ['Basic_roomA','Basic_roomB','Basic_roomC','Basic_roomABC_unchange1']:
 assert levels.load_level('/HorrorSystems/Maps/'+name)
 for p in actors.get_all_level_actors():
  if isinstance(p,u.HSPickup) and p.required_story_step>=0:
   item=p.item_data;mat=item.inspection_materials[0];p.mesh.set_material(0,mat)
   # Paper front normal is local X; pitch puts the paper flat on the tabletop.
   location=p.get_actor_location();b=p.mesh.static_mesh.get_bounds();t=p.mesh.get_world_transform()
   old_bottom=min(t.transform_location(b.origin+u.Vector(i*b.box_extent.x,j*b.box_extent.y,k*b.box_extent.z)).z for i in (-1,1) for j in (-1,1) for k in (-1,1))
   p.set_actor_rotation(u.Rotator(pitch=90,yaw=0,roll=0),False);t=p.mesh.get_world_transform()
   new_bottom=min(t.transform_location(b.origin+u.Vector(i*b.box_extent.x,j*b.box_extent.y,k*b.box_extent.z)).z for i in (-1,1) for j in (-1,1) for k in (-1,1))
   p.set_actor_location(location+u.Vector(0,0,old_bottom-new_bottom),False,False)
 assert levels.save_current_level()
u.log('JAM_CLUE_TEXTURES_BOUND')
