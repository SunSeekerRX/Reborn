import unreal as u
from pathlib import Path
import json

eal=u.EditorAssetLibrary
root='/HorrorSystems/Art'
report={'materials':[], 'meshes':[], 'actors':[]}
u.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(['/HorrorSystems'],force_rescan=True)

for index in range(2):
    mat=eal.load_asset(f'{root}/Locker/M_Locker_{index}')
    assert mat
    expressions=u.MaterialEditingLibrary.get_material_expressions(mat)
    row={'path':mat.get_path_name(),'expressions':[]}
    for expression in expressions:
        item={'path':expression.get_path_name(),'class':expression.get_class().get_name()}
        if isinstance(expression,u.MaterialExpressionTextureSample):
            item['texture']=expression.texture.get_path_name() if expression.texture else None
            item['const_coordinate']=expression.get_editor_property('const_coordinate')
        row['expressions'].append(item)
    report['materials'].append(row)

paths=[f'{root}/Locker/SM_Locker',f'{root}/Fitted/SM_Locker_08f4a9a9fb47']
for path in paths:
    mesh=eal.load_asset(path)
    assert mesh
    bounds=mesh.get_bounds()
    report['meshes'].append({'path':mesh.get_path_name(),'origin':str(bounds.origin),'extent':str(bounds.box_extent),'slots':[{'name':str(s.material_slot_name),'material':s.material_interface.get_path_name() if s.material_interface else None} for s in mesh.static_materials]})

level_sub=u.get_editor_subsystem(u.LevelEditorSubsystem)
actor_sub=u.get_editor_subsystem(u.EditorActorSubsystem)
assert level_sub.load_level('/HorrorSystems/Maps/Basic_roomB')
for actor in actor_sub.get_all_level_actors():
    for component in actor.get_components_by_class(u.StaticMeshComponent):
        mesh=component.static_mesh
        if mesh and 'SM_Locker' in mesh.get_name():
            report['actors'].append({'label':actor.get_actor_label(),'mesh':mesh.get_path_name(),'transform':str(actor.get_actor_transform()),'component_transform':str(component.get_world_transform()),'overrides':[m.get_path_name() if m else None for m in component.get_editor_property('override_materials')],'effective_materials':[component.get_material(i).get_path_name() if component.get_material(i) else None for i in range(component.get_num_materials())]})

report_name = 'LockerAfterRepair.json' if '-LockerAfterRepair' in u.SystemLibrary.get_command_line() else 'LockerBeforeRepair.json'
out=Path(u.Paths.project_dir())/'work'/report_name
out.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
u.log('LOCKER_INSPECTION_COMPLETE '+str(out))
