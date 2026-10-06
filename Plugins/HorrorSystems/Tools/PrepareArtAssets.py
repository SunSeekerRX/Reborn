import unreal as u
from pathlib import Path
import json

root='/HorrorSystems'
u.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([root],force_rescan=True)
eal=u.EditorAssetLibrary;assets=u.AssetToolsHelpers.get_asset_tools();mel=u.MaterialEditingLibrary
source=Path(u.Paths.project_dir(),'ArtSource')
out=Path(u.Paths.project_saved_dir(),'Verification');out.mkdir(exist_ok=True)

# Outline uses custom stencil, so it also works on generated blockout materials.
name='M_InteractionOutline';path=root+'/Materials/'+name
mat=eal.load_asset(path) if eal.does_asset_exist(path) else assets.create_asset(name,root+'/Materials',u.Material,u.MaterialFactoryNew())
mat.set_editor_property('material_domain',u.MaterialDomain.MD_POST_PROCESS)
mat.set_editor_property('blendable_priority',10)
mel.delete_all_material_expressions(mat)
def node(cls):return mel.create_material_expression(mat,cls,0,0)
def wire(a,b,input_name,output=''):assert mel.connect_material_expressions(a,output,b,input_name)
def scalar(value):
    a=node(u.MaterialExpressionConstant);a.r=value;return a
def mask(a,output='Color'):
    m=node(u.MaterialExpressionComponentMask);m.set_editor_property('r',True);m.set_editor_property('g',False);m.set_editor_property('b',False);m.set_editor_property('a',False);wire(a,m,'',output);return m
def texture(kind):
    n=node(u.MaterialExpressionSceneTexture);n.set_editor_property("scene_texture_id",kind);return n
stencil=texture(u.SceneTextureId.PPI_CUSTOM_STENCIL);center=mask(stencil)
uv=node(u.MaterialExpressionScreenPosition)
maximum=center
for dx,dy in [(1,0),(-1,0),(0,1),(0,-1)]:
    delta=node(u.MaterialExpressionConstant2Vector);delta.r=dx*2;delta.g=dy*2
    offset=node(u.MaterialExpressionMultiply);wire(stencil,offset,'A','InvSize');wire(delta,offset,'B')
    add=node(u.MaterialExpressionAdd);wire(uv,add,'A','ViewportUV');wire(offset,add,'B')
    neighbor=texture(u.SceneTextureId.PPI_CUSTOM_STENCIL);wire(add,neighbor,'UVs')
    largest=node(u.MaterialExpressionMax);wire(maximum,largest,'A');wire(mask(neighbor),largest,'B');maximum=largest
diff=node(u.MaterialExpressionSubtract);wire(maximum,diff,'A');wire(center,diff,'B')
edge=node(u.MaterialExpressionSaturate);wire(diff,edge,'')
minus=node(u.MaterialExpressionSubtract);wire(maximum,minus,'A');wire(scalar(1),minus,'B')
division=node(u.MaterialExpressionDivide);wire(minus,division,'A');wire(scalar(254),division,'B')
color_alpha=node(u.MaterialExpressionSaturate);wire(division,color_alpha,'')
yellow=node(u.MaterialExpressionConstant3Vector);yellow.constant=u.LinearColor(1,.65,.06,1)
blue=node(u.MaterialExpressionConstant3Vector);blue.constant=u.LinearColor(.02,.65,1,1)
color=node(u.MaterialExpressionLinearInterpolate);wire(yellow,color,'A');wire(blue,color,'B');wire(color_alpha,color,'Alpha')
scene=texture(u.SceneTextureId.PPI_POST_PROCESS_INPUT0)
rgb=node(u.MaterialExpressionComponentMask)
for channel in ('r','g','b'):rgb.set_editor_property(channel,True)
rgb.set_editor_property('a',False);wire(scene,rgb,'','Color')
blend=node(u.MaterialExpressionLinearInterpolate);wire(rgb,blend,'A');wire(color,blend,'B');wire(edge,blend,'Alpha')
assert mel.connect_material_property(blend,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(mat);assert eal.save_loaded_asset(mat)

rows=json.loads((source/'Inventory.json').read_text(encoding='utf-8'))
for row in rows:
    row['path']=str(source/row['group']/row['original'])
report=[]
for group in ('Bookshelf','Drawer','Sofa','Locker','Butcher'):
    row=next(r for r in rows if r['group']==group and r['path'].lower().endswith('.fbx'))
    task=u.AssetImportTask();task.filename=row['path'];task.destination_path=root+'/Art/'+group
    task.destination_name='SM_'+group;task.automated=True;task.replace_existing=False;task.save=True
    options=u.FbxImportUI();options.import_mesh=True;options.import_as_skeletal=False
    options.mesh_type_to_import=u.FBXImportType.FBXIT_STATIC_MESH
    options.automated_import_should_detect_type=False;options.import_materials=False;options.import_textures=False
    options.static_mesh_import_data.combine_meshes=True;options.static_mesh_import_data.auto_generate_collision=True
    task.options=options
    if not eal.does_asset_exist(task.destination_path+'/'+task.destination_name):assets.import_asset_tasks([task])
    mesh=eal.load_asset(task.destination_path+'/'+task.destination_name)
    assert isinstance(mesh,u.StaticMesh),(group,task.imported_object_paths)
    report.append({'group':group,'mesh':mesh.get_path_name(),'bounds':str(mesh.get_bounds()),'materials':[str(s.material_slot_name) for s in mesh.static_materials]})
    for i,row in enumerate(r for r in rows if r['group']==group and r['path'].lower().endswith(('.png','.jpg','.jpeg'))):
        task=u.AssetImportTask();task.filename=row['path'];task.destination_path=root+'/Art/'+group;task.destination_name='T_'+group+'_'+str(i)
        task.automated=True;task.replace_existing=False;task.save=True
        if not eal.does_asset_exist(task.destination_path+'/'+task.destination_name):assets.import_asset_tasks([task])
        report.append({'group':group,'texture':task.destination_path+'/'+task.destination_name,'original':row['original']})

sounds={'脚步':'S_PlayerSteps','怪脚':'S_MonsterSteps','主角压力':'S_PlayerPressure','环境背景音':'S_AmbientBackground','第一阶段':'S_MonsterEntrance','第二阶段':'S_Stage2Music'}
seen=set()
for row in rows:
    if not row['path'].lower().endswith('.wav'):continue
    name=next((name for token,name in sounds.items() if token in row['original']),None)
    if not name or name in seen:continue
    seen.add(name)
    task=u.AssetImportTask();task.filename=row['path'];task.destination_path=root+'/Audio/Imported';task.destination_name=name
    task.automated=True;task.replace_existing=False;task.save=True
    if not eal.does_asset_exist(task.destination_path+'/'+name):assets.import_asset_tasks([task])
    sound=eal.load_asset(task.destination_path+'/'+name);assert sound
    sound.set_editor_property('looping',name in ('S_AmbientBackground','S_Stage2Music','S_PlayerPressure'))
    assert eal.save_loaded_asset(sound)
    report.append({'sound':sound.get_path_name(),'original':row['original']})
for name in ('S_PlayerStepSingle','S_MonsterStepSingle'):
    task=u.AssetImportTask();task.filename=str(source/'Audio/Derived'/(name+'.wav'))
    task.destination_path=root+'/Audio/Imported';task.destination_name=name
    task.automated=True;task.replace_existing=False;task.save=True
    assert Path(task.filename).is_file(),task.filename
    if not eal.does_asset_exist(task.destination_path+'/'+name):assets.import_asset_tasks([task])
    assert eal.load_asset(task.destination_path+'/'+name)
manny=eal.load_asset(root+'/Characters/Mannequins/Meshes/SKM_Manny_Simple')
assert manny,'Manny reference mesh is missing'
task=u.AssetExportTask();task.object=manny;task.filename=str(out/'MannyRigSource.fbx')
task.automated=True;task.prompt=False;task.replace_identical=True
task.options=u.FbxExportOption();task.options.export_local_time=True;task.options.bake_material_inputs=u.FbxMaterialBakeMode.DISABLED
if not Path(task.filename).exists():assert u.Exporter.run_asset_export_task(task),'Manny reference FBX export failed'
(out/'ArtImportInventory.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
u.log('REBORN_ART_ASSETS_PREPARED')
