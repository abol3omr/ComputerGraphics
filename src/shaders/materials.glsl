// Materials: what each surface is made of.
// The notation follows the Phong reflection model from the lecture.

struct Material {
  vec3 k;      // material color, used as both the ambient (ka) and diffuse (kd) color
  float ks;    // specular reflection coefficient
  float alpha; // shininess exponent: higher = smaller, sharper highlight
};

// --- stone ---------------------------------------------------------------------

uniform int uStone;  // 0 white marble, 1 black marble, 2 green marble, 3 travertine
uniform float uSlab; // which part of the "quarry" the slab is cut from

struct Stone {
  vec3 vein;        // color of the veins
  vec3 base;        // color of the stone between them
  float frequency;  // how many veins per metre
  float amount;     // how strongly the turbulence bends the veins
  float veinWidth;  // 0..1: small = thin sharp veins, large = soft wide bands
};

Stone getStone() {
  if (uStone == 1) return Stone(vec3(0.78, 0.62, 0.34), vec3(0.025, 0.025, 0.03), 6.0, 4.0, 0.10);
  if (uStone == 2) return Stone(vec3(0.66, 0.80, 0.72), vec3(0.04, 0.19, 0.13), 9.0, 4.0, 0.15);
  if (uStone == 3) return Stone(vec3(0.55, 0.42, 0.29), vec3(0.78, 0.68, 0.54), 30.0, 1.6, 1.00);
  return Stone(vec3(0.30, 0.32, 0.37), vec3(0.86, 0.85, 0.82), 9.0, 3.5, 0.13);
}

// The palette: maps a value in [-1, 1] to a color between the vein and the base.
vec3 marble_color(float x, Stone stone) {
  float t = 0.5 + 0.5 * x;
  return mix(stone.vein, stone.base, smoothstep(0.0, stone.veinWidth, t));
}

// The marble function from the lecture:
//
//   function marble(p)
//     x = p.x + turbulence(p);
//     return marble_color(sin(x))
//
// sin(p.x) alone gives straight parallel stripes; adding turbulence bends them into veins.
// Here the stripes run along a diagonal direction instead of x, so they cross the
// countertop at an angle, and each stone type has its own frequency and amount.
//
// The texture is a function of the 3D point itself, in metres. There are no UV
// coordinates, so it cannot stretch: a longer countertop simply shows more stone.
vec3 marble(vec3 p) {
  Stone stone = getStone();
  p += uSlab * vec3(3.1, 1.7, 2.3); // cut the slab from a different place
  float stripes = dot(p, vec3(0.55, 1.0, 1.0));
  float x = stone.frequency * stripes + stone.amount * turbulence(2.2 * p);
  vec3 color = marble_color(sin(x), stone);
  return color * (0.94 + 0.06 * FBm(9.0 * p)); // faint cloudy variation in the base
}

// --- the room --------------------------------------------------------------------

uniform int uFloor;         // 0 wood planks, 1 tiles, 2 concrete
uniform vec3 uWallColor;
uniform vec3 uCabinetColor;

// A pseudo-random number in [0, 1] for a grid cell, from the same hash as the noise.
float random(ivec2 cell) {
  return float(hash(uint(cell.x) + hash(uint(cell.y))) & 0xffffU) / 65535.0;
}

// Distance from a point inside a rectangular cell to the nearest side of the cell.
float distanceToCellEdge(vec2 local, vec2 size) {
  return min(min(local.x, size.x - local.x), min(local.y, size.y - local.y));
}

// Wood planks. The floor is divided into planks by repeating the coordinates with
// floor() and fract(). Each plank gets its own random number, used to stagger the rows,
// tint the plank and choose where in the log it was cut from.
//
// The grain is the wood function from the lecture:
//
//   function wood(p)
//     x = (p.x^2 + p.y^2) + FBm(p);
//     return wood_color(sin(x))
//
// p.x^2 + p.y^2 is the squared distance from the axis of the log, so sin() of it gives
// the growth rings, and FBm makes them irregular.
Material woodPlanks(vec3 p) {
  const vec2 PLANK = vec2(1.20, 0.14); // length and width of a plank, in metres
  float row = floor(p.z / PLANK.y);
  float stagger = random(ivec2(int(row), 7)) * PLANK.x;
  float column = floor((p.x + stagger) / PLANK.x);
  float r = random(ivec2(int(row), int(column)));
  vec2 local = vec2(fract((p.x + stagger) / PLANK.x), fract(p.z / PLANK.y)) * PLANK;

  // Position of this point inside the log: q.xy across the rings, q.z along the trunk.
  vec3 q = vec3(local.y + 0.25 + 0.5 * r, 0.3 + r, 0.08 * p.x + 10.0 * r);
  float x = 55.0 * (q.x * q.x + q.y * q.y) + 1.2 * FBm(vec3(6.0 * q.xy, 4.0 * q.z));
  vec3 color = mix(vec3(0.22, 0.12, 0.06), vec3(0.45, 0.29, 0.16), 0.5 + 0.5 * sin(x)); // wood_color
  color *= 0.80 + 0.40 * r;

  // A dark gap between planks.
  color *= mix(0.35, 1.0, smoothstep(0.0, 0.004, distanceToCellEdge(local, PLANK)));
  return Material(color, 0.12, 25.0);
}

// Square tiles with grout lines. Each tile has a slightly different shade, and FBm
// adds a faint cloudy variation inside it.
Material tiles(vec3 p) {
  const vec2 TILE = vec2(0.60, 0.60);
  ivec2 cell = ivec2(floor(p.xz / TILE));
  vec2 local = fract(p.xz / TILE) * TILE;
  vec3 color = vec3(0.56, 0.54, 0.50) * (0.93 + 0.10 * random(cell)) * (0.96 + 0.06 * FBm(4.0 * p));
  float grout = smoothstep(0.002, 0.006, distanceToCellEdge(local, TILE));
  return Material(mix(vec3(0.20, 0.19, 0.18), color, grout), 0.35 * grout, 60.0);
}

// Poured concrete: large soft patches from FBm plus fine grain from high-frequency noise.
Material concrete(vec3 p) {
  vec3 color = vec3(0.33, 0.33, 0.32) * (0.90 + 0.14 * FBm(1.5 * p) + 0.05 * Noise(60.0 * p));
  return Material(color, 0.05, 10.0);
}

// The cabinet: a painted box with thin dark gaps between the doors on its front face.
Material cabinet(vec3 p) {
  vec3 color = uCabinetColor;
  float frontZ = WALL_Z + uDepth - SLAB_OVERHANG;
  if (abs(p.z - frontZ) < 0.002) {
    float cabinetLength = uLength - 2.0 * SLAB_OVERHANG;
    float doorWidth = cabinetLength / max(1.0, floor(cabinetLength / 0.55 + 0.5)); // doors of about 55 cm
    float x = p.x + cabinetLength / 2.0;
    float distanceToGap = abs(fract(x / doorWidth + 0.5) - 0.5) * doorWidth;
    color *= mix(0.35, 1.0, smoothstep(0.002, 0.007, distanceToGap));
  }
  return Material(color, 0.15, 30.0);
}

Material getMaterial(float id, vec3 p) {
  if (id == MATERIAL_FLOOR) {
    if (uFloor == 1) return tiles(p);
    if (uFloor == 2) return concrete(p);
    return woodPlanks(p);
  }
  if (id == MATERIAL_WALL) return Material(uWallColor, 0.00, 1.0);
  if (id == MATERIAL_CABINET) return cabinet(p);
  if (id == MATERIAL_STONE) return Material(marble(p), 0.60, 90.0);
  return Material(vec3(0.55, 0.22, 0.13), 0.35, 50.0); // vase
}
