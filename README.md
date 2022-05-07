# scad2nodes
Code to convert an OpenSCAD model to a tree of Blender Geometry Nodes

This repo contains experimental code to convert an OpenSCAD model to a tree of Blender geometry nodes

Repo also contains a bunch of example models to try it on.

The state of this project is very much alpha / in progress, in particular:
  - it must be run from the command line and is not yet fully integrated into Blender as a plugin
  - not all OpenSCAD primitives are properly supported yet

Reason for this experiment:
  . This was built in the context of the geometry-as-code experiment (see https://www.youtube.com/watch?v=aZq8ZlmqHJo )
  . The current geometry-as-code node operates by calling OpenSCAD externally to fully render the model, which is kind of slow
  . The idea here is to try to express an OpenSCAD object as a native tree of Blender boolean operations
  . The blender CSG engine is quite a bit faster than OpenSCAD's (but less robust), which may in future provide a "fast path" for experimentation / model building, with a switch to full-blown OpenSCAD "rendering" once the model is ready.
  . It's a fun and very fast way to instantiate arbitrarily large geometry node trees in blender

Usage:

  . make sure you have blender v3.0 installed
  . make sure you have a C++-17 compiler and make installed
  . make sure you have a recent version of OpenSCAD installed

  git clone https://github.com/emogenet/scad2nodes
  cd scad2nodes
  make

If you want to try a model that's known to work:

  cp hull.py z.py
  ./py.py

The last step will launch blender and execute the python code in py.py that will create the geometry nodes tree.

If you want to see the tree, select the one object in the scene and open a Geometry Node panel

If you want to try your own model (disclaimer: not all OpenSCAD functionalities are coded yet)
  cp <some/openscad/path/to/mymodel.scad> .
  make
  cp mymodel.py z.py
  ./py.py

Couple of things worth noting:

  If you see something along the lines of "RuntimeError: Error: Node type GeometryNodeMeshBoolean undefined" when you run py.py, it means your version of Blender is too old. Install v3.1 at least.

  Some OpenSCAD primitives require the creation of BMesh objects external to the node tree (eg polygon primitive). The code creates objects in that case and "injects" them into the tree via "Object Info" nodes

  There is a Blender native add-on called 'Node Arrange' . I suggest you activate it as it will help automatically layout the sometimes very large trees produced by this project. It will take a very long time when the tree is very large, but it is nevertheless useful.

  parse.cpp is not very sophisticated, but not completely naive either. It comes with:
    - a "dead code  elimination" pass that will try and spot useless portions of the CSG tree and remove them
    - an "identical subtree packer" that will try and spot identical subtrees in the CSG tree and merge them using a merkle-tree like approach
    - a "transform packer" that will try and spot long strings of 4x4 transforms in the CSG tree and concatenate them into a single transform node

  Overview of how this works:
      MymOdel.scad -> OpenSCAD -> MyModel.csg file -> parse.cpp -> MyModel.py (Blender python code) -> Blender

  There is quite a bit more work to be done on this:
    - need to make sure all arguments of primitives are taken into account (eg linear_extrude is not done in that regard)
    - automatic layout of the tree when building it would be nice rather than relying on the Arrange Node add-on
    - add missing primitives (eg polyehedron, rotate_extrude, minkowski)
    - fine-tune some primitives (eg. text)
    - testing

  enhancements and patches are most welcome

