from pathlib import Path
exec((Path(__file__).parent/'ApplyNewRoomArt.py').read_text(encoding='utf-8').split('# Imported brass')[0])
patched=[]
for name in ('Basic_roomA','Basic_roomB','Basic_roomC','Basic_roomABC'):
    assert levels.load_level(root+'/Maps/'+name)
    for actor in actors.get_all_level_actors():
        if not actor.get_actor_label().startswith('CubeGrid'):continue
        c=actor.get_component_by_class(u.StaticMeshComponent)
        for i in range(len(c.static_mesh.static_materials)):
            if not c.get_material(i):
                c.set_material(i,wall_material(eal.load_asset('/Engine/EngineMaterials/WorldGridMaterial'),name));patched.append([name,actor.get_actor_label(),i])
    assert levels.save_current_level()
(saved/'WallMaterialCompletion.json').write_text(json.dumps(patched,indent=2),encoding='utf-8')
u.log('WALL_MATERIALS_COMPLETE')
