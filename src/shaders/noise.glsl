// Noise functions from the "Procedural Textures" lecture.
// The function names follow the pseudocode on the slides: Noise, FBm, turbulence.

const int OCTAVES = 6; // the "N" in "Do N times"

// "Not feasible to store values at all integer locations ... use a randomized hash
// function to map lattice locations to pseudo-random values."
uint hash(uint x) {
  x ^= x >> 16;
  x *= 0x7feb352dU;
  x ^= x >> 15;
  x *= 0x846ca68bU;
  x ^= x >> 16;
  return x;
}

// A pseudo-random unit gradient for a grid point.
vec3 gradient(ivec3 c) {
  uint h1 = hash(uint(c.x) + hash(uint(c.y) + hash(uint(c.z))));
  uint h2 = hash(h1);
  uint h3 = hash(h2);
  vec3 g = vec3(float(h1 & 0xffffU), float(h2 & 0xffffU), float(h3 & 0xffffU)) / 32767.5 - 1.0;
  return normalize(g);
}

// Classic Perlin (gradient) noise in 3D, roughly in [-1, 1].
// Random gradients on the grid points, Hermite-interpolated between them. It needs only
// the 2^3 nearest gradients, and its value is 0 at every integer grid point.
float Noise(vec3 p) {
  ivec3 i = ivec3(floor(p));
  vec3 f = fract(p);
  vec3 u = f * f * (3.0 - 2.0 * f); // Hermite blend

  float n000 = dot(gradient(i + ivec3(0, 0, 0)), f - vec3(0, 0, 0));
  float n100 = dot(gradient(i + ivec3(1, 0, 0)), f - vec3(1, 0, 0));
  float n010 = dot(gradient(i + ivec3(0, 1, 0)), f - vec3(0, 1, 0));
  float n110 = dot(gradient(i + ivec3(1, 1, 0)), f - vec3(1, 1, 0));
  float n001 = dot(gradient(i + ivec3(0, 0, 1)), f - vec3(0, 0, 1));
  float n101 = dot(gradient(i + ivec3(1, 0, 1)), f - vec3(1, 0, 1));
  float n011 = dot(gradient(i + ivec3(0, 1, 1)), f - vec3(0, 1, 1));
  float n111 = dot(gradient(i + ivec3(1, 1, 1)), f - vec3(1, 1, 1));

  return mix(
    mix(mix(n000, n100, u.x), mix(n010, n110, u.x), u.y),
    mix(mix(n001, n101, u.x), mix(n011, n111, u.x), u.y),
    u.z
  ) * 1.6;
}

// Fractal Brownian Motion: a sum of scaled copies of the noise,
// each with a higher frequency and a lower amplitude.
//
//   function FBm(p)
//     t = 0; scale = 1;
//     Do N times
//       t += Noise(p/scale)*scale;
//       scale /= 2;
//     return t;
float FBm(vec3 p) {
  float t = 0.0;
  float scale = 1.0;
  for (int i = 0; i < OCTAVES; i++) {
    t += Noise(p / scale) * scale;
    scale /= 2.0;
  }
  return t;
}

// Turbulence: "same as FBm, but sum absolute value of noise function".
// The absolute value creates sharp creases, which is what makes veins look like veins.
float turbulence(vec3 p) {
  float t = 0.0;
  float scale = 1.0;
  for (int i = 0; i < OCTAVES; i++) {
    t += abs(Noise(p / scale)) * scale;
    scale /= 2.0;
  }
  return t;
}
