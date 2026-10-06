import unreal as u
from pathlib import Path
import json
root='/HorrorSystems';project=Path(u.Paths.project_dir());eal=u.EditorAssetLibrary
u.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([root],force_rescan=True)
(project/'Saved/Verification').mkdir(parents=True,exist_ok=True)
assets=u.AssetToolsHelpers.get_asset_tools();mel=u.MaterialEditingLibrary
rows=json.loads((project/'ArtSource/NewRoomArt/Inventory.json').read_text(encoding='utf-8'));report=[]
for group in ('SafeWall','RoomWall','DiningTable','Lantern','BookshelfNew'):
    files=[r for r in rows if r['group']==group]
    images=[r for r in files if Path(r['file']).suffix.lower() in ('.jpg','.png')]
    image=next((r for r in images if '墙' in r['original']),images[0])
    task=u.AssetImportTask();task.filename=str(project/image['file']);task.destination_path=root+'/Art/'+group;task.destination_name='T_'+group
    task.automated=True;task.replace_existing=False;task.save=True
    if not eal.does_asset_exist(task.destination_path+'/'+task.destination_name):assets.import_asset_tasks([task])
    texture=eal.load_asset(task.destination_path+'/'+task.destination_name);assert texture
    if group in ('SafeWall','RoomWall'):
        report.append({'group':group,'texture':texture.get_path_name()});continue
    fbx=next(r for r in files if Path(r['file']).suffix.lower()=='.fbx')
    task=u.AssetImportTask();task.filename=str(project/fbx['file']);task.destination_path=root+'/Art/'+group;task.destination_name='SM_'+group
    task.automated=True;task.replace_existing=False;task.save=True
    options=u.FbxImportUI();options.import_mesh=True;options.import_as_skeletal=False;options.import_materials=False;options.import_textures=False
    options.mesh_type_to_import=u.FBXImportType.FBXIT_STATIC_MESH;options.automated_import_should_detect_type=False
    options.static_mesh_import_data.combine_meshes=True;options.static_mesh_import_data.auto_generate_collision=False
    task.options=options
    if not eal.does_asset_exist(task.destination_path+'/'+task.destination_name):assets.import_asset_tasks([task])
    mesh=eal.load_asset(task.destination_path+'/'+task.destination_name);assert mesh
    path=root+'/Art/'+group+'/M_'+group
    mat=eal.load_asset(path) if eal.does_asset_exist(path) else assets.create_asset('M_'+group,root+'/Art/'+group,u.Material,u.MaterialFactoryNew())
    mel.delete_all_material_expressions(mat)
    sample=mel.create_material_expression(mat,u.MaterialExpressionTextureSample,0,0);sample.texture=texture
    mel.connect_material_property(sample,'RGB',u.MaterialProperty.MP_BASE_COLOR)
    rough=mel.create_material_expression(mat,u.MaterialExpressionConstant,0,150);rough.r=.7
    mel.connect_material_property(rough,'',u.MaterialProperty.MP_ROUGHNESS)
    mat.set_editor_property('two_sided',True);mel.recompile_material(mat);assert eal.save_loaded_asset(mat)
    for i in range(len(mesh.static_materials)):mesh.set_material(i,mat)
    mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    assert eal.save_loaded_asset(mesh)
    report.append({'group':group,'mesh':mesh.get_path_name(),'bounds':str(mesh.get_bounds()),'materials':[str(s.material_slot_name) for s in mesh.static_materials]})
(project/'Saved/Verification/NewRoomArtImport.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
u.log('NEW_ROOM_ART_IMPORTED')
