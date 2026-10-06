"""Generate portable inspection meshes and configure existing item assets."""
import unreal as u
from pathlib import Path
import math

source = Path(u.Paths.project_dir()) / "Plugins/HorrorSystems/Tools/InspectionMeshSource"
source.mkdir(parents=True, exist_ok=True)
assets = u.AssetToolsHelpers.get_asset_tools()
eal = u.EditorAssetLibrary
mel = u.MaterialEditingLibrary
folder = "/HorrorSystems/Items/Inspection"
u.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(["/HorrorSystems"], force_rescan=True)

class Model:
    def __init__(self):
        self.lines = []
        self.index = 0
    def face(self, points, uv=None):
        uv = uv or ([(.02,.02),(.08,.02),(.08,.08),(.02,.08)] if len(points)==4 else [(.02,.02),(.08,.02),(.08,.08)])
        indices = []
        for point, tex in zip(points, uv):
            self.index += 1
            self.lines.append("v %.6f %.6f %.6f" % tuple(point))
            self.lines.append("vt %.6f %.6f" % tuple(tex))
            indices.append(str(self.index) + "/" + str(self.index))
        self.lines.append("f " + " ".join(indices))
    def box(self, center, extent, front_texture=False):
        x,y,z = center
        a,b,c = extent
        self.face([(x+a,y-b,z-c),(x+a,y+b,z-c),(x+a,y+b,z+c),(x+a,y-b,z+c)],
                  [(0,0),(1,0),(1,1),(0,1)] if front_texture else None)
        self.face([(x-a,y+b,z-c),(x-a,y-b,z-c),(x-a,y-b,z+c),(x-a,y+b,z+c)])
        self.face([(x-a,y-b,z-c),(x+a,y-b,z-c),(x+a,y-b,z+c),(x-a,y-b,z+c)])
        self.face([(x+a,y+b,z-c),(x-a,y+b,z-c),(x-a,y+b,z+c),(x+a,y+b,z+c)])
        self.face([(x-a,y-b,z+c),(x+a,y-b,z+c),(x+a,y+b,z+c),(x-a,y+b,z+c)])
        self.face([(x-a,y+b,z-c),(x+a,y+b,z-c),(x+a,y-b,z-c),(x-a,y-b,z-c)])
    def cylinder(self, radius, low, high, along_x=False):
        def p(angle, depth):
            a,b = radius*math.cos(angle),radius*math.sin(angle)
            return (depth,a,b) if along_x else (a,b,depth)
        for i in range(40):
            a,b = i*math.tau/40,(i+1)*math.tau/40
            self.face([p(a,low),p(b,low),p(b,high),p(a,high)])
            center_low = (low,0,0) if along_x else (0,0,low)
            center_high = (high,0,0) if along_x else (0,0,high)
            self.face([center_low,p(b,low),p(a,low)])
            self.face([center_high,p(a,high),p(b,high)])
    def ring(self):
        def p(a,b):
            r = 7 + 1.2*math.cos(b)
            return (1.2*math.sin(b),r*math.cos(a),10+r*math.sin(a))
        for i in range(40):
            for j in range(10):
                a,b,c,d = i*math.tau/40,(i+1)*math.tau/40,j*math.tau/10,(j+1)*math.tau/10
                self.face([p(a,c),p(b,c),p(b,d),p(a,d)])
    def save(self, name):
        path = source / (name + ".obj")
        path.write_text("\n".join(self.lines) + "\n",encoding="utf-8")
        return path

models = {}
note = Model(); note.box((0,0,0),(.22,18,11.2),True); models["Note"] = note
key = Model(); key.ring(); key.box((0,0,-6),(1.2,1.2,11))
key.box((0,2,-10),(1.2,3,1.2)); key.box((0,2,-15),(1.2,3,1.2)); models["Key"] = key
battery = Model(); battery.cylinder(6,-15,15); battery.cylinder(2,15,17); models["Battery"] = battery
token = Model(); token.cylinder(10,-1,1,True); token.box((1.25,0,0),(.3,1,5)); models["Token"] = token

tasks = []
for name, model in models.items():
    path = model.save("SM_Inspect" + name)
    if name=="Note" or not eal.does_asset_exist(folder + "/" + path.stem):
        task = u.AssetImportTask()
        task.filename = str(path); task.destination_path = folder; task.destination_name = path.stem
        task.automated = True; task.replace_existing = True; task.save = True
        tasks.append(task)
assets.import_asset_tasks(tasks)

def material(name, color, metallic, texture=None):
    path = folder + "/" + name
    mat = eal.load_asset(path) if eal.does_asset_exist(path) else assets.create_asset(name,folder,u.Material,u.MaterialFactoryNew())
    mat.set_editor_property('used_with_nanite',True)
    mel.delete_all_material_expressions(mat)
    if texture:
        c = mel.create_material_expression(mat,u.MaterialExpressionTextureSample,0,0)
        c.texture = texture
        mel.connect_material_property(c,"RGB",u.MaterialProperty.MP_BASE_COLOR)
    else:
        c = mel.create_material_expression(mat,u.MaterialExpressionConstant3Vector,0,0)
        c.constant = u.LinearColor(*color,1)
        mel.connect_material_property(c,"",u.MaterialProperty.MP_BASE_COLOR)
    for prop, value in [(u.MaterialProperty.MP_METALLIC,metallic),(u.MaterialProperty.MP_ROUGHNESS,.6)]:
        v = mel.create_material_expression(mat,u.MaterialExpressionConstant,0,180)
        v.r = value; mel.connect_material_property(v,"",prop)
    mel.recompile_material(mat); eal.save_loaded_asset(mat)
    return mat

colors = {"Note":(.8,.74,.6),"Key":(.38,.24,.08),"Battery":(.15,.2,.23),"Token":(.5,.035,.025)}
for name in models:
    item = eal.load_asset("/HorrorSystems/Items/DA_" + name)
    mesh = eal.load_asset(folder + "/SM_Inspect" + name)
    assert item and isinstance(mesh,u.StaticMesh), "Inspection mesh import failed: " + name
    mat = material("M_Inspect" + name,colors[name],0 if name=="Note" else .65,
                   item.inspection_image if name=="Note" else None)
    mesh.set_material(0,mat)
    item.set_editor_property("inspection_mesh",mesh)
    item.set_editor_property("inspection_materials",[mat])
    item.set_editor_property("inspection_scale",u.Vector(1,1,1))
    item.set_editor_property("inspection_rotation",u.Rotator(0,-15,0))
    eal.save_loaded_asset(mesh); eal.save_loaded_asset(item)
u.log("INSPECTION_3D_MODELS_CONFIGURED")

# Runtime Slate material: SceneColor capture stores inverse opacity in alpha.
ui_name = "M_InspectionPreviewUI"
ui_path = folder + "/" + ui_name
ui_mat = eal.load_asset(ui_path) if eal.does_asset_exist(ui_path) else assets.create_asset(ui_name,folder,u.Material,u.MaterialFactoryNew())
ui_mat.set_editor_property("material_domain",u.MaterialDomain.MD_UI)
ui_mat.set_editor_property("blend_mode",u.BlendMode.BLEND_TRANSLUCENT)
mel.delete_all_material_expressions(ui_mat)
sample = mel.create_material_expression(ui_mat,u.MaterialExpressionTextureSampleParameter2D,0,0)
sample.set_editor_property("parameter_name","PreviewTexture")
sample.texture = eal.load_asset("/HorrorSystems/UI/T_Note")
alpha = mel.create_material_expression(ui_mat,u.MaterialExpressionOneMinus,220,160)
assert mel.connect_material_expressions(sample,"A",alpha,"")
mel.connect_material_property(alpha,"",u.MaterialProperty.MP_OPACITY)
add = mel.create_material_expression(ui_mat,u.MaterialExpressionAdd,220,0)
add.set_editor_property("const_b",1.0)
mel.connect_material_expressions(sample,"RGB",add,"A")
divide = mel.create_material_expression(ui_mat,u.MaterialExpressionDivide,440,0)
mel.connect_material_expressions(sample,"RGB",divide,"A")
mel.connect_material_expressions(add,"",divide,"B")
mel.connect_material_property(divide,"",u.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(ui_mat); eal.save_loaded_asset(ui_mat)
u.log("INSPECTION_TRANSPARENT_UI_CONFIGURED")
