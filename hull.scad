
// a minimal rounded corner box using hull

$fn = 30;

module vertex() {
  sphere(r=0.2);
}

module edge() {
  union() {
    translate([0,  1, 0]) vertex();
    translate([0, -1, 0]) vertex();
  }
}

module square() {
  translate([ 1, 0, 0]) edge();
  translate([-1, 0, 0]) edge();
}

module cube() {
    translate([0, 0,  1]) square();
    translate([0, 0, -1]) square();
}

hull() {
  cube();
}

