// Materials: what each surface is made of.
// The notation follows the Phong reflection model from the lecture.

struct Material {
  vec3 k;      // material color, used as both the ambient (ka) and diffuse (kd) color
  float ks;    // specular reflection coefficient
  float alpha; // shininess exponent: higher = smaller, sharper highlight
};

Material getMaterial(float id, vec3 p) {
  if (id == MATERIAL_FLOOR) return Material(vec3(0.42, 0.29, 0.19), 0.10, 20.0);
  if (id == MATERIAL_WALL) return Material(vec3(0.66, 0.64, 0.60), 0.00, 1.0);
  if (id == MATERIAL_CABINET) return Material(vec3(0.10, 0.16, 0.21), 0.15, 30.0);
  if (id == MATERIAL_STONE) return Material(vec3(0.62, 0.62, 0.60), 0.60, 90.0);
  return Material(vec3(0.55, 0.22, 0.13), 0.35, 50.0); // vase
}
