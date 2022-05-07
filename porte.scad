
// hörmann-style door

$fn = 20;

xDim =  110;
yDim = 1620;
zDim = 2510;
frameThickness = 100;

doorYDim = 1100;
doorZDim = zDim - frameThickness;

windowXDim = 5;
windowYDim = (yDim - doorYDim -3*frameThickness);
windowZDim = (zDim - 2*frameThickness);

module door(
  tScale = 1.0
) {
  translate([0, frameThickness, 0])
  cube([tScale * xDim, doorYDim, doorZDim]);
}

module mainShape() {
  difference() {

    cube([xDim, yDim, zDim]);

    translate([-500, 0, 0])
    door(tScale = 10.0);

    translate([-5000, yDim - windowYDim - frameThickness, frameThickness])
    scale([10, 1, 1])
    cube([10000, windowYDim, zDim - 2*frameThickness]);
  }
}

module window() {
  translate([0, (yDim - windowYDim - frameThickness), frameThickness])
  cube([windowXDim, windowYDim, windowZDim]);
}

module windows() {
  translate([40, 0, 0]) window();
  translate([xDim - 40, 0, 0]) window();
}

hXDim = 250;
hYDim = 250;
hZDim = hYDim;
nH = 6;
tHZ = (10 + hYDim);
tHZDim = (tHZ * nH);

module hublot(
  xScale = 1.0
) {
  translate([0, frameThickness + doorYDim/2 - hYDim/2, 0])
  cube([(hXDim * xScale), hYDim, hZDim]);
}

module hublots(
  xScale = 1.0
) {
  base = (doorZDim/2 - tHZDim/2);
  translate([0, 0, 0*tHZ + base]) hublot(xScale);
  translate([0, 0, 1*tHZ + base]) hublot(xScale);
  translate([0, 0, 2*tHZ + base]) hublot(xScale);
  translate([0, 0, 3*tHZ + base]) hublot(xScale);
  translate([0, 0, 4*tHZ + base]) hublot(xScale);
  translate([0, 0, 5*tHZ + base]) hublot(xScale);
}

module handleHolder() {
  color([0.75, 0.75, 0.75, 1.0])
  translate([0, 60 + 920, 680])
  rotate([0, 90, 0])
  cylinder(d = 15, h = 150);
}

module lock() {
  translate([0, 1100, doorZDim/2])
  rotate([0, 90, 0])
  difference() {
    cylinder(d = 80, h = 100);
    translate([0, 0, 100]) cube([50, 10, 160], center = true);
  }
}

module completeDoor() {

  color([0.5, 0.5, 0.5, 1.0])
  lock();

  // holders
  translate([0, 0, 1*tHZ]) handleHolder();
  translate([0, 0, 3*tHZ]) handleHolder();

  // big handle
  color([0.75, 0.75, 0.75, 1.0])
  translate([170, 60 + 920, 680])
  cylinder(d = 50, h = 4 * tHZ);

  // back windows for hublots
  color([0, 0, 1, 0.2])
  translate([10, 0, 0])
  hublots(xScale = 0.02);

  // front windows for hublots
  color([0, 0, 1, 0.2])
  translate([xDim-35, 0, 0])
  hublots(xScale = 0.02);

  // door, with holes for hublots
  difference() {
    color([1, 1, 1, 1]) door(tScale = 90/110);
    color([0, 1, 1, 0]) translate([-100, 0, 0]) hublots();
  }
}

module frame() {
  color([0, 0, 1, 0.2]) windows();
  color([1, 0, 0, 1.0]) mainShape();
}

module assembly() {
  frame();
  completeDoor();
}

scale([0.001, 0.001, 0.001])
assembly();

