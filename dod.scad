
// a bunch of dodecahedrons placed on a spiral with a hollowed out cylindrical core

radius = 2.0;
nbSteps = 100;
zIncrPerStep = 0.1;
angleIncrPerStep = 30.0;

module dodecahedron() {
  r = 1.0;
  phi = atan(sqrt(5.0)/2.0 - 0.5);
  hull() {
    for(a=[0:72:288]) {
      rotate([phi, 0, a])
      translate([-r/2.0, -r/2.0, -r/2.0])
      cube(r);
    }
  }
}

// random rotations
randVect = rands(
    0.0,
  360.0,
  3*nbSteps,
  12345
);

module walls() {
  for(step=[0:1:nbSteps]) {
    translate([0, 0, step*zIncrPerStep])
    rotate([0, 0, step*angleIncrPerStep])
    translate([radius, 0, 0])
    rotate([
      randVect[2*step + 0],
      randVect[2*step + 1],
      randVect[2*step + 3]
    ])
    dodecahedron();
  }
}

module hole() {
  cylinder(
    center = true,
    r = radius - 0.25,
    h = 100.0,
    $fn = 50
  );
}

difference() {
  walls();
  hole();
}

