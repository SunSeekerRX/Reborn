import unreal as u
from pathlib import Path
import json, math

Q=u.GeometryScript_MeshQueries
M=u.GeometryScript_Materials
def vector(v):return [v.x,v.y,v.z]
def mesh_data(asset):
    dm=u.DynamicMesh()
    _,result=u.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(asset,dm,u.GeometryScriptCopyMeshFromAssetOptions(),u.GeometryScriptMeshReadLOD())
    assert result==u.GeometryScriptOutcomePins.SUCCESS
    triangles=[]
    hist={}
    count_fn=getattr(Q,next(name for name in dir(Q) if name.startswith('get_num_triangle')))
    for tid in range(count_fn(dm)):
        valid,a,b,c=Q.get_triangle_positions(dm,tid)
        if not valid:continue
        material,valid=M.get_triangle_material_id(dm,tid)
        assert valid
        va,vb,vc=vector(a),vector(b),vector(c)
        ab=[vb[i]-va[i] for i in range(3)];ac=[vc[i]-va[i] for i in range(3)]
        cross=[ab[1]*ac[2]-ab[2]*ac[1],ab[2]*ac[0]-ab[0]*ac[2],ab[0]*ac[1]-ab[1]*ac[0]]
        area=math.sqrt(sum(x*x for x in cross))/2
        normal=[x/(area*2) if area>1e-12 else 0 for x in cross]
        dominant=max(range(3),key=lambda i:abs(normal[i]))
        key=str(material)+':'+('+' if normal[dominant]>0 else '-')+'XYZ'[dominant]
        hist[key]=hist.get(key,0)+area
        triangles.append({'id':tid,'material':material,'positions':[va,vb,vc],'normal':normal,'area':area})
    bounds=asset.get_bounds()
    return {'asset':asset.get_path_name(),'origin':vector(bounds.origin),'extent':vector(bounds.box_extent),'areas_by_material_and_direction':hist,'triangles':triangles}

u.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(['/HorrorSystems'],force_rescan=True)
paths=['/HorrorSystems/Art/Locker/SM_Locker']
paths += ['/HorrorSystems/Art/Fitted/SM_Locker_'+suffix for suffix in ['08f4a9a9fb47','fe3a33fd7f56','139fe6f35804']]
rows=[mesh_data(u.EditorAssetLibrary.load_asset(path)) for path in paths]
out=Path(u.Paths.project_dir())/'work/LockerFacingBefore.json'
out.write_text(json.dumps(rows,ensure_ascii=False),encoding='utf-8')
for row in rows:
    u.log('LOCKER_FACING '+json.dumps({k:v for k,v in row.items() if k!='triangles'},ensure_ascii=False))
