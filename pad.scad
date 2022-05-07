
// door lock pad

$fn = 60;

x = 61.0;   // plate dimension in X (width)
y = 28.0;   // plate dimension in 4 (height)
z = 12.0;   // plate dimension in Z (thickness) -- door to outside
r =  1.5;   // radius of corners

hx = 11.0;  // hole X
hy = 15.0;  // hole Y
hd =  4.5;  // hole diameter
hh = 80.0;  // hole "height"

chx = x/2.0;// center hole X
chy =  8.0; // center hole X
chd =   hd; // center hole diameter
chh =   hh; // center hole "height"

csd = 5.5;  // counter-sunk diameter
csh = 1.0;  // counter-sunk height

module roundedBox0(
  size = [1, 1, 1],
  radius = 0.1,
  sidesOnly = false
) {
  hull() {
    for(z = [-size[2]/2, size[2]/2]) {
      for(y = [-size[1]/2, size[1]/2]) {
        for(x = [-size[0]/2, size[0]/2]) {
          translate([
            x - radius*sign(x),
            y - radius*sign(y),
            z - radius*sign(z)
          ])
          if(sidesOnly) {
            cylinder(
              r = radius,
              h = 2*radius,
              center=true
            );
          } else {
            sphere(
              r = radius,
              center=true
            );
          }
        }
      }
    }
  }
}

module roundedBox(
  size = [1, 1, 1],
  radius = 0.1,
  sidesOnly = false,
  topOnly = false
) {
  if(false==topOnly) {
    roundedBox0(
      size = size,
      radius = radius,
      sidesOnly = sidesOnly
    );
  } else {
    translate([0, 0, -size[2]/2])
    difference() {
      roundedBox0(
        size = [
          size[0],
          size[1],
          2*size[2]
        ],
        radius = radius,
        sidesOnly = sidesOnly
      );
      scale([4*size[0], 4*size[1], 4*size[2]])
      translate([-0.5, -0.5, -1])
      cube();
    }
  }
}


// base rounded box shape
module box() {
  translate([x/2.0, y/2.0, z/2.0])
  roundedBox(
    size = [x, y, z],
    radius = r,
    sidesOnly = true,
    topOnly = true
  );
}

// hole with a scaling factor
module hole(
    s = 1.0
) {
  translate([hx, hy, 0])
  scale([s, s, 1.0])
  cylinder(
    d = hd,
    h = hh,
    center = true
  );
}

// simple hole in the center
module centerHole() {
  translate([chx, chy,0])
  cylinder(
    d = chd,
    h = chh,
    center = true
  );
}

// side holes
module holes(
    s = 1.0
) {
                             hole(s = s);
  translate([x-hx-hx, 0, 0]) hole(s = s);
}

// counter-sunk around side holes
module counterSunkHole() {
  translate([0, 0, csh/2.0])
  scale([1, 1, csh/hh])
  holes(s = (csd/hd));
}

module boredBox() {
  difference() {
    union() {
      box();
      translate([0, 0, -csh + 1/100]) counterSunkHole();
    }
    translate([0, 0, z-csh + 1/100]) counterSunkHole();
    centerHole();
    holes();
  }
}

boredBox();


