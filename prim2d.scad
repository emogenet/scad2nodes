
if(true) {
  translate([-5, 10, 10])
  circle(d=4, $fn=32);

  translate([10, 5, 0])
  square([1, 2]);

  polygon(
    points = [
      [0, 0],
      [1, 0],
      [2, 1],
      [3, 3],
      [2, 4],
      [-10, 10],
      [2, 3]
    ]
  );
}

translate([10, 10, 10]) text("blargh");

linear_extrude() {
  text("extruded");
}

