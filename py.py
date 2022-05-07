#!/usr/bin/env -S blender --python
#!/usr/bin/env -S blender --factory-startup --python
#!/usr/bin/env -S blender -b --factory-startup --python

# things we need
# --------------
import re
import bpy
import sys
import math
import mathutils

# add a polygon
# -------------
def make_polygon(
  vertices
):

  vertices = [[float(v[0]), float(v[1]), 0.0] for v in vertices]

  mesh = bpy.data.meshes.new("scad_poly")
  obj = bpy.data.objects.new(mesh.name, mesh)
  col = bpy.data.collections.get("Collection")
  col.objects.link(obj)

  saved = bpy.context.view_layer.objects.active
  bpy.context.view_layer.objects.active = obj

  edges = []
  faces = [list(range(0, len(vertices)))]
  mesh.from_pydata(vertices, edges, faces)
  bpy.context.view_layer.objects.active = saved

  obj.hide_viewport = True
  obj.hide_render = True
  obj.hide_set(True)
  return obj

# start with a clean scene
# ------------------------
bpy.context.preferences.view.show_splash = False
for obj in bpy.data.objects:
    bpy.data.objects.remove(obj, do_unlink = True)

# set 3d view clipping to something reasonable
# --------------------------------------------
for a in bpy.context.screen.areas:
    if a.type == 'VIEW_3D':
        for s in a.spaces:
            if s.type == 'VIEW_3D':
                s.clip_end = 100000
                s.clip_start = 0.01

# create a plane, select it, add geometry node modifier
# -----------------------------------------------------
bpy.ops.mesh.primitive_plane_add()
bpy.ops.object.select_all(action='SELECT')
obj = bpy.context.active_object
mod = obj.modifiers.new("GeometryNodes", 'NODES')

# create new geometry node group with no input node and one output node
# ---------------------------------------------------------------------
group = bpy.data.node_groups.new("Geometry Nodes", 'GeometryNodeTree')
group.outputs.new('NodeSocketGeometry', "Geometry")
output_node = group.nodes.new('NodeGroupOutput')
output_node.is_active_output = True
output_node.select = False
mod.node_group = group

# search for a node by type in a tree
# -----------------------------------
def get_node_index(
  nodes,
  datatype
):
  idx = 0
  for m in nodes:
    if (m.type == datatype):
      return idx
    idx += 1
  return 1 # by default

# create or get a simple mat for the 'color' OpenSCAD operator
# ------------------------------------------------------------
colorMaterials = {}
def getColorMat(
  rgba
):

  name = str(rgba)
  #name = re.sub(r'\s', '_', name)
  #name = re.sub(r'\.', 'D', name)
  name = re.sub(r'^', 'col_', name)

  if name in colorMaterials:
    return colorMaterials[name]

  mat = colorMaterials[name] = bpy.data.materials.new(name)

  rgba = [float(x) for x in rgba]
  mat.diffuse_color = rgba
  mat.use_nodes = True

  nodes = mat.node_tree.nodes
  node = nodes.new('ShaderNodeBsdfDiffuse')
  node.name = f'Auto-generated diffuse bsdf for color {name}'
  node.inputs[0].default_value = rgba
  out = node.outputs[0]
  inn = nodes[get_node_index(nodes, 'OUTPUT_MATERIAL')].inputs[0]
  mat.node_tree.links.new(out, inn)
  return mat

# quick and dirty arg parsing for OpenSCAD operators
# --------------------------------------------------
def parseArgs(
  args
):
  args = re.sub(r'\$', '_ss_', args)
  args = re.sub(r'undef', 'None', args)
  code = f'''
def evalArgs(local_dict, *args, **kwargs):
  i = 0
  for arg in args:
    local_dict['arg_%d' % i] = arg
    i += 1
  local_dict.update(kwargs)

true = True
false = False
evalArgs(locals(), {args})
'''
  result = {}
  exec(code, None, result)
  return result

def getGeomOutput(
  node
):
  for output in node.outputs:
    if 'GEOMETRY'==output.type:
        return output
  raise Exception('node has no geometry output')

# convert a SCAD operator to Blender geometry node
# ------------------------------------------------
def Node(
  name,
  args,
  inputNodes,
  code_line
):

  node = None
  global group
  args = parseArgs(args)

  if 'sphere'==name:
    fn = int(args['_ss_fn'])
    radius = float(args['r'])
    sphere = group.nodes.new('GeometryNodeMeshUVSphere')
    sphere.inputs['Radius'].default_value = radius
    sphere.inputs['Segments'].default_value = fn
    sphere.inputs['Rings'].default_value = fn
    node = sphere

  elif 'cube'==name:
    center = args['center']
    size = [float(x) for x in args['size']]
    cube = group.nodes.new('GeometryNodeMeshCube')
    cube.inputs['Size'].default_value = size
    node = cube

    if center is False:
      offset = [x/2.0 for x in size]
      transform = group.nodes.new('GeometryNodeTransform')
      transform.inputs['Translation'].default_value = offset
      group.links.new(transform.inputs[0], getGeomOutput(cube))
      node = transform

  elif 'cylinder'==name:
    h = float(args['h'])
    r1 = float(args['r1'])
    r2 = float(args['r2'])
    center = args['center']
    fn = int(args['_ss_fn'])
    cylinder = group.nodes.new('GeometryNodeMeshCone')
    cylinder.inputs['Depth'].default_value = h
    cylinder.inputs['Radius Top'].default_value = r1
    cylinder.inputs['Radius Bottom'].default_value = r2
    cylinder.inputs['Vertices'].default_value = fn
    node = cylinder

    if center is True:
      offset = [0.0, 0.0, -h/2.0]
      transform = group.nodes.new('GeometryNodeTransform')
      transform.inputs['Translation'].default_value = offset
      group.links.new(transform.inputs[0], getGeomOutput(cylinder))
      node = transform

  elif 'multmatrix'==name:

    # https://docs.blender.org/api/current/mathutils.html#mathutils.Matrix
    m = args['arg_0']
    bm = mathutils.Matrix()
    for i in range(0, 4):
      for j in range(0, 4):
        bm[i][j] = m[i][j]
    t, q, s = bm.decompose()
    r = q.to_euler()

    transform = group.nodes.new('GeometryNodeTransform')
    transform.inputs['Translation'].default_value = t
    transform.inputs['Rotation'].default_value = r
    transform.inputs['Scale'].default_value = s
    if 0<len(inputNodes):
      group.links.new(transform.inputs[0], getGeomOutput(inputNodes[0]))
    node = transform

  elif 'color'==name: # TODO: this doesn't surve boolean ops
    color = group.nodes.new('GeometryNodeSetMaterial')
    color.inputs['Material'].default_value = getColorMat(args['arg_0'])
    if 0<len(inputNodes):
      group.links.new(color.inputs[0], getGeomOutput(inputNodes[0]))
    node = color

  elif 'difference'==name:
    difference = group.nodes.new('GeometryNodeMeshBoolean')
    if 0<len(inputNodes):
      group.links.new(difference.inputs[0], getGeomOutput(inputNodes[0]))
    for inputNode in inputNodes[1:]:
      group.links.new(difference.inputs[1], getGeomOutput(inputNode))
    node = difference

  elif ('union'==name) or ('group'==name):
    union = group.nodes.new('GeometryNodeMeshBoolean')
    union.operation = 'UNION'
    for inputNode in inputNodes:
      group.links.new(union.inputs[1], getGeomOutput(inputNode))
    node = union

  elif ('intersection'==name):
    intersection = group.nodes.new('GeometryNodeMeshBoolean')
    intersection.operation = 'INTERSECT'
    for inputNode in inputNodes:
      group.links.new(intersection.inputs[1], getGeomOutput(inputNode))
    node = intersection

  elif ('text'==name):

    text = args['text']
    string2Curve = group.nodes.new('GeometryNodeStringToCurves')
    string2Curve.inputs['String'].default_value = text
    node = string2Curve

    if True:
      fillCurve = group.nodes.new('GeometryNodeFillCurve')
      fillCurve.mode = 'NGONS'
      group.links.new(fillCurve.inputs[0], getGeomOutput(string2Curve))
      node = fillCurve

  elif ('circle'==name):

    radius = float(args['r'])
    circle = group.nodes.new('GeometryNodeCurvePrimitiveCircle')
    circle.inputs['Radius'].default_value = radius
    node = circle

    if True:
      fillCurve = group.nodes.new('GeometryNodeFillCurve')
      fillCurve.mode = 'NGONS'
      group.links.new(fillCurve.inputs[0], getGeomOutput(circle))
      node = fillCurve

  elif ('square'==name):

    size = args['size']
    center = args['center']
    square = group.nodes.new('GeometryNodeCurvePrimitiveQuadrilateral')
    square.inputs['Width'].default_value = float(size[0])
    square.inputs['Height'].default_value = float(size[1])
    node = square

    if True:
      fillCurve = group.nodes.new('GeometryNodeFillCurve')
      fillCurve.mode = 'NGONS'
      group.links.new(fillCurve.inputs[0], getGeomOutput(square))
      node = fillCurve

  elif ('polygon'==name):
    points = args['points']
    poly_obj = make_polygon(points)
    poly_node = group.nodes.new('GeometryNodeObjectInfo')
    poly_node.inputs[0].default_value = poly_obj
    node = poly_node

  elif ('linear_extrude'==name):

    height = float(args['height'])
    extrudeMesh = group.nodes.new('GeometryNodeExtrudeMesh')
    extrudeMesh.inputs['Individual'].default_value = False
    extrudeMesh.inputs['Offset Scale'].default_value = height
    if 0<len(inputNodes):
      group.links.new(extrudeMesh.inputs[0], getGeomOutput(inputNodes[0]))

    union = group.nodes.new('GeometryNodeMeshBoolean')
    union.operation = 'UNION'
    if 0<len(inputNodes):
      group.links.new(union.inputs[1], getGeomOutput(inputNodes[0]))
    group.links.new(union.inputs[1], getGeomOutput(extrudeMesh))

    transform = group.nodes.new('GeometryNodeTransform')
    transform.inputs['Translation'].default_value = [0, 0, height/2];
    group.links.new(transform.inputs[0], getGeomOutput(union))
    node = transform

  elif 'hull'==name:
    hull = group.nodes.new('GeometryNodeConvexHull')
    if 1<len(hull.inputs):
      union = group.nodes.new('GeometryNodeMeshBoolean')
      union.operation = 'UNION'
      for inputNode in inputNodes:
        group.links.new(union.inputs[1], getGeomOutput(inputNode))
      group.links.new(hull.inputs[0], getGeomOutput(union))
    else:
      if 0<len(inputNodes):
        group.links.new(hull.inputs[0], getGeomOutput(inputNodes[0]))
    node = hull

  elif 'minkowski'==name:
    print(f'unimplemented operator {name}, replace with hull')
    mink = group.nodes.new('GeometryNodeConvexHull')
    if 1<len(mink.inputs):
      union = group.nodes.new('GeometryNodeMeshBoolean')
      union.operation = 'UNION'
      for inputNode in inputNodes:
        group.links.new(union.inputs[1], getGeomOutput(inputNode))
      group.links.new(mink.inputs[0], getGeomOutput(union))
    else:
      if 0<len(inputNodes):
        group.links.new(mink.inputs[0], getGeomOutput(inputNodes[0]))
    node = mink

  else:
    print(f'unknown operator {name}, replace with cube')
    node = group.nodes.new('GeometryNodeMeshCube')
    node.inputs['Size'].default_value = [1, 1, 1]

  return node

exec(open('z.py').read())
group.links.new(output_node.inputs[0], getGeomOutput(output))
if False:
  try:
    pass
  except Exception:
    print(f'failed to load z.py')
    sys.exit(1)

# frame selection in all views
# -----------------------------
for a in bpy.context.screen.areas:
    if a.type == 'VIEW_3D':
        ctx = bpy.context.copy()
        ctx['area'] = a
        ctx['region'] = a.regions[-1]
        bpy.ops.view3d.view_selected(ctx)

