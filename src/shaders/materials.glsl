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

Material getMaterial(float id, vec3 p) {
  if (id == MATERIAL_FLOOR) return Material(vec3(0.42, 0.29, 0.19), 0.10, 20.0);
  if (id == MATERIAL_WALL) return Material(vec3(0.66, 0.64, 0.60), 0.00, 1.0);
  if (id == MATERIAL_CABINET) return Material(vec3(0.10, 0.16, 0.21), 0.15, 30.0);
  if (id == MATERIAL_STONE) return Material(marble(p), 0.60, 90.0);
  return Material(vec3(0.55, 0.22, 0.13), 0.35, 50.0); // vase
}
