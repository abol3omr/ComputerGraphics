// Lighting: the Phong reflection model from the lecture, plus two effects that a ray
// marcher gets almost for free because it can ask "how far is the nearest surface?"
// at any point: soft shadows and ambient occlusion.

uniform float uShadows;          // 1 = on, 0 = off
uniform float uAmbientOcclusion; // 1 = on, 0 = off

// One parallel (directional) light source, the sun. Its direction and color depend on
// the time of day and are computed in sun.ts.
uniform vec3 uLightDirection; // direction TO the light
uniform vec3 La; // ambient light intensity
uniform vec3 Ld; // diffuse light intensity
uniform vec3 Ls; // specular light intensity

// Soft shadow: march from the surface point towards the light.
// If the ray hits something, the point is in full shadow (0). If it only passes close
// to an object, the point is in the penumbra: the closer the miss (small h) and the
// nearer it is to the surface point (small t), the darker. `sharpness` controls how
// quickly the shadow fades out.
float softShadow(vec3 p, vec3 l) {
  const float sharpness = 10.0;
  float light = 1.0;
  float t = 0.02;
  for (int i = 0; i < 64; i++) {
    float h = map(p + l * t).x;
    if (h < 0.0005) return 0.0;
    light = min(light, sharpness * h / t);
    t += clamp(h, 0.01, 0.20);
    if (t > 8.0) break;
  }
  return clamp(light, 0.0, 1.0);
}

// Ambient occlusion: corners and creases receive less ambient light.
// Step a few short distances d along the normal. In open space the SDF there equals d.
// If it is smaller, some other surface is nearby and is blocking part of the sky.
float ambientOcclusion(vec3 p, vec3 n) {
  float occlusion = 0.0;
  float weight = 1.0;
  for (int i = 1; i <= 5; i++) {
    float d = 0.035 * float(i);
    occlusion += weight * (d - map(p + n * d).x);
    weight *= 0.6;
  }
  return clamp(1.0 - 3.0 * occlusion, 0.0, 1.0);
}

// Phong reflection model:
//
//   l - direction to light source     n - normal direction
//   v - direction to the eye (COP)    r - direction of reflected ray
//
//   I  = Ia + Id + Is
//   Ia = La * ka
//   Id = kd * (l . n) * Ld
//   Is = ks * (r . v)^alpha * Ls
//
// The shadow multiplies the diffuse and specular terms (light that comes straight from
// the source). The ambient occlusion multiplies the ambient term (light from everywhere).
vec3 phongReflection(vec3 p, vec3 n, vec3 v, Material m) {
  vec3 l = normalize(uLightDirection);
  vec3 r = 2.0 * n * dot(n, l) - l;

  float shadow = mix(1.0, softShadow(p + n * 0.002, l), uShadows);
  float occlusion = mix(1.0, ambientOcclusion(p, n), uAmbientOcclusion);

  float ln = max(dot(l, n), 0.0);
  vec3 Ia = La * m.k * occlusion;
  vec3 Id = m.k * ln * Ld * shadow;
  vec3 Is = ln > 0.0 ? m.ks * pow(max(dot(r, v), 0.0), m.alpha) * Ls * shadow : vec3(0.0);
  return Ia + Id + Is;
}
