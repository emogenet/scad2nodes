
$fn = 10;

module hole() {
  scale([2.0, 0.5, 0.5]) sphere();
}

module thingie() {
  difference() {
    sphere();
    hole();
    rotate([0, 0, 90]) hole();
    rotate([0, 90, 0]) hole();
  }
}

union() {
  for(i = [0 : 5] ) {
    r0 = 180.0*sin(1000*i*cos(1000*i*sin(300*i)));
    r1 = 180.0*sin(2000*i*cos(3000*i*sin(400*i)));
    r2 = 180.0*sin(3000*i*cos(4000*i*sin(500*i)));
    translate([i, 0, 0])
    rotate([r0, r1, r2])
    thingie();
  }
}

