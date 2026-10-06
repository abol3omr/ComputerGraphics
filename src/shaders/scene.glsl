// The scene: a countertop on a cabinet, against a wall. Units are metres, y is up.

const float MATERIAL_FLOOR = 0.0;
const float MATERIAL_WALL = 1.0;
const float MATERIAL_CABINET = 2.0;
const float MATERIAL_STONE = 3.0;
const float MATERIAL_VASE = 4.0;

const float WALL_Z = -0.90;         // the wall is the plane z = WALL_Z
const float CABINET_HEIGHT = 0.86;
const float CABINET_HALF_WIDTH = 1.10;
const float CABINET_DEPTH = 0.60;
const float SLAB_THICKNESS = 0.04;  // a 4 cm countertop
const float SLAB_OVERHANG = 0.03;   // the slab sticks out 3 cm past the cabinet

// Keep whichever of the two (distance, material) pairs is closer.
vec2 closer(vec2 a, vec2 b) {
  return a.x < b.x ? a : b;
}

// Returns vec2(distance to the nearest surface, material of that surface).
vec2 map(vec3 p) {
  // Floor: the plane y = 0. Wall: the plane z = WALL_Z.
  vec2 result = vec2(p.y, MATERIAL_FLOOR);
  result = closer(result, vec2(p.z - WALL_Z, MATERIAL_WALL));

  // Cabinet: a box standing on the floor with its back against the wall.
  vec3 cabinetCentre = vec3(0.0, CABINET_HEIGHT / 2.0, WALL_Z + CABINET_DEPTH / 2.0);
  vec3 cabinetHalfSize = vec3(CABINET_HALF_WIDTH, CABINET_HEIGHT / 2.0, CABINET_DEPTH / 2.0);
  result = closer(result, vec2(sdBox(p - cabinetCentre, cabinetHalfSize), MATERIAL_CABINET));

  // Countertop: a thin box on top of the cabinet, overhanging the front and the sides.
  float slabDepth = CABINET_DEPTH + SLAB_OVERHANG;
  vec3 slabCentre = vec3(0.0, CABINET_HEIGHT + SLAB_THICKNESS / 2.0, WALL_Z + slabDepth / 2.0);
  vec3 slabHalfSize = vec3(CABINET_HALF_WIDTH + SLAB_OVERHANG, SLAB_THICKNESS / 2.0, slabDepth / 2.0);
  float stone = sdBox(p - slabCentre, slabHalfSize);

  // Backsplash: a strip of the same stone on the wall behind the countertop.
  vec3 splashCentre = vec3(0.0, CABINET_HEIGHT + SLAB_THICKNESS + 0.28, WALL_Z + 0.01);
  stone = min(stone, sdBox(p - splashCentre, vec3(slabHalfSize.x, 0.28, 0.01)));
  result = closer(result, vec2(stone, MATERIAL_STONE));

  // Vase: two spheres blended with a smooth minimum. It gives a sense of scale now,
  // and later something for the polished stone to reflect.
  float top = CABINET_HEIGHT + SLAB_THICKNESS;
  float body = sdSphere(p - vec3(-0.60, top + 0.12, -0.62), 0.12);
  float neck = sdSphere(p - vec3(-0.60, top + 0.28, -0.62), 0.045);
  result = closer(result, vec2(smoothMin(body, neck, 0.09), MATERIAL_VASE));

  return result;
}
