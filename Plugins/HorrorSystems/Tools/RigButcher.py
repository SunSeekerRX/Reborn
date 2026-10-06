"""Blender background: bind supplied unrigged butcher to the existing Manny rig."""
import bpy
from pathlib import Path
from mathutils import Vector
import json
import math

project=Path(__file__).resolve().parents[3]
source=project/'ArtSource/Butcher/butcher.fbx'
reference=project/'Saved/Verification/MannyRigSource.fbx'
target=project/'ArtSource/Butcher/SKM_Butcher.fbx'
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(reference))
armature=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE')
manny=next(o for o in bpy.context.scene.objects if o.type=='MESH')
# Blender represents the FBX root joint as the armature object. Restore it as a
# real bone so UE can import the exact root -> pelvis hierarchy.
if not armature.data.bones.get('root'):
    bpy.ops.object.select_all(action='DESELECT');armature.select_set(True);bpy.context.view_layer.objects.active=armature
    bpy.ops.object.mode_set(mode='EDIT')
    roots=[b for b in armature.data.edit_bones if b.parent is None]
    root_bone=armature.data.edit_bones.new('root');root_bone.head=(0,0,0);root_bone.tail=(0,1,0)
    for b in roots:b.parent=root_bone
    bpy.ops.object.mode_set(mode='OBJECT')
before=set(bpy.context.scene.objects)
bpy.ops.import_scene.fbx(filepath=str(source))
pieces=[o for o in bpy.context.scene.objects if o not in before and o.type=='MESH']
assert pieces,'Supplied butcher contains no mesh'
for o in pieces:
    matrix=o.matrix_world.copy();o.parent=None;o.matrix_world=matrix
bpy.ops.object.select_all(action='DESELECT')
for o in pieces:o.select_set(True)
bpy.context.view_layer.objects.active=pieces[0]
bpy.ops.object.join()
butcher=bpy.context.object;butcher.name='Butcher'
bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)

def bounds(obj):
    points=[obj.matrix_world@Vector(v) for v in obj.bound_box]
    return Vector([min(p[i] for p in points) for i in range(3)]),Vector([max(p[i] for p in points) for i in range(3)])
lo,hi=bounds(butcher);rlo,rhi=bounds(manny)
if ((hi.x-lo.x)>(hi.y-lo.y)) != ((rhi.x-rlo.x)>(rhi.y-rlo.y)):
    # Supplied mesh faces -X; Manny faces +Y. Rotate clockwise before skinning.
    butcher.rotation_euler.z=-math.pi/2
    bpy.context.view_layer.update();bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    lo,hi=bounds(butcher)
scale=(rhi.z-rlo.z)/(hi.z-lo.z)
butcher.scale=Vector((scale,scale,scale));bpy.context.view_layer.update()
lo,hi=bounds(butcher)
butcher.location=Vector(((rlo.x+rhi.x-lo.x-hi.x)*.5,(rlo.y+rhi.y-lo.y-hi.y)*.5,rlo.z-lo.z))
bpy.context.view_layer.update();bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
for g in manny.vertex_groups:
    if not butcher.vertex_groups.get(g.name):butcher.vertex_groups.new(name=g.name)
transfer=butcher.modifiers.new('Manny surface skin weights','DATA_TRANSFER')
transfer.object=manny;transfer.use_vert_data=True;transfer.data_types_verts={'VGROUP_WEIGHTS'}
transfer.vert_mapping='POLYINTERP_NEAREST'
bpy.ops.object.modifier_apply(modifier=transfer.name)
# Normalize transferred weights and ensure no vertices remain unbound.
fallback=butcher.vertex_groups.get('pelvis')
unbound=0
for vertex in butcher.data.vertices:
    total=sum(g.weight for g in vertex.groups)
    if total<.00001:
        fallback.add([vertex.index],1,'REPLACE');unbound+=1
    elif abs(total-1)>.001:
        for g in list(vertex.groups):butcher.vertex_groups[g.group].add([vertex.index],g.weight/total,'REPLACE')
modifier=butcher.modifiers.new('Manny Armature','ARMATURE');modifier.object=armature
world=butcher.matrix_world.copy();butcher.parent=armature;butcher.matrix_world=world
armature.name='Armature'
bpy.ops.object.select_all(action='DESELECT');butcher.select_set(True);armature.select_set(True)
bpy.context.view_layer.objects.active=armature
bpy.ops.export_scene.fbx(filepath=str(target),use_selection=True,add_leaf_bones=False,bake_anim=False,
    object_types={'ARMATURE','MESH'},axis_forward='-Z',axis_up='Y',path_mode='STRIP',mesh_smooth_type='FACE')
(project/'Saved/Verification/ButcherRig.json').write_text(json.dumps({'vertices':len(butcher.data.vertices),'bones':len(armature.data.bones),'groups':len(butcher.vertex_groups),'fallback_vertices':unbound,'materials':[m.name for m in butcher.data.materials],'height':rhi.z-rlo.z,'file':str(target)},ensure_ascii=False,indent=2),encoding='utf-8')
bpy.ops.wm.save_as_mainfile(filepath=str(project/'ArtSource/Butcher/ButcherRig.blend'))
print('REBORN_BUTCHER_RIGGED',flush=True)
