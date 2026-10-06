"""Restore the locker body texture without replacing its material graph.

Run with the project's Unreal Python commandlet while the editor is closed.
Only M_Locker_0 is saved when its body sample uses the door texture. Backups and
verification records are written to Saved/Verification/LockerMaterialRepair.
"""
import hashlib
import json
import shutil
from datetime import datetime
from pathlib import Path
import unreal as u

PROJECT = Path(u.Paths.project_dir()).resolve()
ROOT = '/HorrorSystems/Art/Locker'
OUT = PROJECT / 'Saved/Verification/LockerMaterialRepair'
OUT.mkdir(parents=True, exist_ok=True)
EAL = u.EditorAssetLibrary
MEL = u.MaterialEditingLibrary

def object_path(obj):
    return obj.get_path_name() if obj else None

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

u.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(
    ['/HorrorSystems'], force_rescan=True)
materials = [EAL.load_asset(f'{ROOT}/M_Locker_{i}') for i in range(2)]
textures = [EAL.load_asset(f'{ROOT}/T_Locker_{i}') for i in range(2)]
assert all(materials) and all(textures), 'Required locker assets are missing'

samples = []
for mat in materials:
    found = [expr for expr in MEL.get_material_expressions(mat)
             if isinstance(expr, u.MaterialExpressionTextureSample)]
    assert len(found) == 1, 'Expected one texture sample; refusing to rebuild material'
    samples.append(found[0])
assert samples[0].texture in textures, 'Unexpected body texture; refusing to replace'
assert samples[1].texture == textures[1], 'Door material requires separate inspection'
assert all(sample.get_editor_property('const_coordinate') == 0 for sample in samples)

target_path = PROJECT / 'Plugins/HorrorSystems/Content/Art/Locker/M_Locker_0.uasset'
before_hash = digest(target_path)
report = {'before_textures': [object_path(s.texture) for s in samples],
          'changed_assets': [], 'backup': None,
          'before_sha256': before_hash}

if samples[0].texture != textures[0]:
    backup_dir = OUT / ('Backup_' + datetime.now().strftime('%Y%m%d_%H%M%S_%f'))
    backup_file = backup_dir / 'Plugins/HorrorSystems/Content/Art/Locker/M_Locker_0.uasset'
    backup_file.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(target_path, backup_file)
    assert digest(backup_file) == before_hash
    report['backup'] = str(backup_dir)
    (backup_dir / 'Manifest.json').write_text(json.dumps({
        'original': str(target_path), 'backup_file': str(backup_file),
        'sha256': before_hash}, indent=2), encoding='utf-8')
    expression_count = MEL.get_num_material_expressions(materials[0])
    samples[0].set_editor_property('texture', textures[0])
    MEL.recompile_material(materials[0])
    assert MEL.get_num_material_expressions(materials[0]) == expression_count
    assert EAL.save_loaded_asset(materials[0]), 'Saving body material failed'
    report['changed_assets'].append(object_path(materials[0]))

assert [s.texture for s in samples] == textures
report['after_textures'] = [object_path(s.texture) for s in samples]
report['after_sha256'] = digest(target_path)

# Check the actual mesh and effective scene materials, including actor overrides.
mesh_path = '/HorrorSystems/Art/Fitted/SM_Locker_08f4a9a9fb47'
mesh = EAL.load_asset(mesh_path)
assert mesh and len(mesh.static_materials) == 2
assert [s.material_interface for s in mesh.static_materials] == materials
report['mesh_materials'] = [object_path(s.material_interface) for s in mesh.static_materials]
levels = u.get_editor_subsystem(u.LevelEditorSubsystem)
actors = u.get_editor_subsystem(u.EditorActorSubsystem)
assert levels.load_level('/HorrorSystems/Maps/Basic_roomB')
report['scene_checks'] = []
for actor in actors.get_all_level_actors():
    for component in actor.get_components_by_class(u.StaticMeshComponent):
        if component.static_mesh != mesh:
            continue
        effective = [component.get_material(i) for i in range(2)]
        assert effective == materials, actor.get_actor_label() + ' has a material override'
        report['scene_checks'].append({'actor': actor.get_actor_label(),
            'materials': [object_path(m) for m in effective],
            'transform': str(actor.get_actor_transform())})
assert len(report['scene_checks']) == 2, 'Expected both affected room B lockers'
report['verified'] = True
(OUT / 'RepairReport.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
u.log('LOCKER_MATERIAL_REPAIR_VERIFIED ' + json.dumps(report, ensure_ascii=False))
