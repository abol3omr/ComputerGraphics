// The scene: a countertop on a cabinet, against a wall. Units are metres, y is up.

const float MATERIAL_FLOOR = 0.0;
const float MATERIAL_WALL = 1.0;
const float MATERIAL_CABINET = 2.0;
const float MATERIAL_STONE = 3.0;
const float MATERIAL_VASE = 4.0;

// The size of the countertop is chosen by the user.
uniform float uLength;    // along the wall
uniform float uDepth;     // from the wall to the front edge
uniform float uThickness; // of the slab

const float WALL_Z = -0.90;         // the wall is the plane z = WALL_Z
const float CABINET_HEIGHT = 0.86;
const float SLAB_OVERHANG = 0.03;   // the slab sticks out 3 cm past the cabinet

uniform int uEdge; // 0 square, 1 bevelled, 2 rounded, 3 bullnose

const float BEVEL_SIZE = 0.014;    // the bevel cuts 14 mm off the top front corner
const float ROUNDED_RADIUS = 0.010; // a 10 mm rounded edge

// The countertop slab with a shaped front edge.
//
// The edge is designed in 2D, as the cross-section you would see if you cut the slab
// and looked at it from the side: z runs towards the front, y runs up. That 2D shape is
// then extruded along x. All four profiles come from the same rounded-rectangle formula:
//   square   - corner radius 0
//   rounded  - a small corner radius
//   bullnose - corner radius = half the thickness, so the front becomes a half circle
//   bevelled - a square profile with the top front corner cut off by a 45 degree plane
float sdSlab(vec3 p) {
  float halfThickness = uThickness / 2.0;
  float halfLength = uLength / 2.0;
  float frontZ = WALL_Z + uDepth;

  // Profile coordinates: y measured from the middle of the slab, z from its front face.
  float y = p.y - (CABINET_HEIGHT + halfThickness);
  float z = p.z - frontZ;

  float radius = 0.0;
  if (uEdge == 2) radius = min(ROUNDED_RADIUS, halfThickness);
  if (uEdge == 3) radius = halfThickness;

  // 2D rounded rectangle: shrink the rectangle by the radius, then grow it back by the
  // radius in every direction, which rounds the corners.
  vec2 q = vec2(z + radius, abs(y) - halfThickness + radius);
  float profile = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;

  if (uEdge == 1) {
    // Intersection of two shapes is max(a, b). Keep only the part of the profile that is
    // behind a 45 degree plane through the top front corner.
    float bevelPlane = (y - halfThickness + z + BEVEL_SIZE) * 0.7071;
    profile = max(profile, bevelPlane);
  }

  // Extrude along x: intersect with the slab's width and with the wall behind it.
  return max(profile, max(abs(p.x) - halfLength, WALL_Z - p.z));
}

// Keep whichever of the two (distance, material) pairs is closer.
vec2 closer(vec2 a, vec2 b) {
  return a.x < b.x ? a : b;
}

// Returns vec2(distance to the nearest surface, material of that surface).
vec2 map(vec3 p) {
  // Floor: the plane y = 0. Wall: the plane z = WALL_Z.
  vec2 result = vec2(p.y, MATERIAL_FLOOR);
  result = closer(result, vec2(p.z - WALL_Z, MATERIAL_WALL));

  // Cabinet: a box standing on the floor with its back against the wall. It follows the
  // size of the countertop, which overhangs it at the front and at both ends.
  float cabinetDepth = uDepth - SLAB_OVERHANG;
  vec3 cabinetCentre = vec3(0.0, CABINET_HEIGHT / 2.0, WALL_Z + cabinetDepth / 2.0);
  vec3 cabinetHalfSize = vec3(uLength / 2.0 - SLAB_OVERHANG, CABINET_HEIGHT / 2.0, cabinetDepth / 2.0);
  result = closer(result, vec2(sdBox(p - cabinetCentre, cabinetHalfSize), MATERIAL_CABINET));

  // Countertop: a slab on top of the cabinet, with the size and the edge profile
  // chosen by the user.
  float stone = sdSlab(p);

  // Backsplash: a strip of the same stone on the wall behind the countertop.
  // It is cut from the same slab, so it has the same thickness. Its edges stay square
  // whatever edge profile is chosen for the countertop.
  vec3 splashCentre = vec3(0.0, CABINET_HEIGHT + uThickness + 0.28, WALL_Z + uThickness / 2.0);
  stone = min(stone, sdBox(p - splashCentre, vec3(uLength / 2.0, 0.28, uThickness / 2.0)));
  result = closer(result, vec2(stone, MATERIAL_STONE));

  // Vase: two spheres blended with a smooth minimum. It gives a sense of scale now,
  // and later something for the polished stone to reflect.
  float top = CABINET_HEIGHT + uThickness;
  vec3 vaseBase = vec3(-0.27 * uLength, top, WALL_Z + 0.28);
  float body = sdSphere(p - vaseBase - vec3(0.0, 0.12, 0.0), 0.12);
  float neck = sdSphere(p - vaseBase - vec3(0.0, 0.28, 0.0), 0.045);
  result = closer(result, vec2(smoothMin(body, neck, 0.09), MATERIAL_VASE));

  return result;
}
