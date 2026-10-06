// Ray marching (sphere tracing) of a signed distance function.
//
// A signed distance function (SDF) returns, for any point p in space, the distance
// from p to the nearest surface: positive outside, negative inside, zero on the surface.
// The scene is not a list of triangles - it is this one function, map(p).

uniform vec2 uResolution;

out vec4 fragColor;

const int MAX_STEPS = 100;
const float MAX_DISTANCE = 50.0;
const float SURFACE_EPSILON = 0.001;

float sdSphere(vec3 p, float radius) {
  return length(p) - radius;
}

// The whole scene: distance from p to the nearest surface.
float map(vec3 p) {
  return sdSphere(p - vec3(0.0, 0.0, 0.0), 1.0);
}

// Walk along the ray. At each point the SDF tells us how far we can safely step
// without passing through any surface, so we step exactly that far.
// Returns the distance travelled, or -1.0 if the ray hit nothing.
float march(vec3 rayOrigin, vec3 rayDirection) {
  float t = 0.0;
  for (int i = 0; i < MAX_STEPS; i++) {
    float distanceToScene = map(rayOrigin + rayDirection * t);
    if (distanceToScene < SURFACE_EPSILON) return t;
    t += distanceToScene;
    if (t > MAX_DISTANCE) break;
  }
  return -1.0;
}

// The normal is the direction in which the distance grows fastest: the gradient of
// the SDF, estimated with central differences.
vec3 calcNormal(vec3 p) {
  vec2 e = vec2(0.001, 0.0);
  return normalize(vec3(
    map(p + e.xyy) - map(p - e.xyy),
    map(p + e.yxy) - map(p - e.yxy),
    map(p + e.yyx) - map(p - e.yyx)
  ));
}

void main() {
  // Pixel -> point on the image plane, with (0,0) in the centre and y in [-1, 1].
  vec2 uv = (2.0 * gl_FragCoord.xy - uResolution) / uResolution.y;

  // A fixed camera on the +z axis looking at the origin.
  vec3 rayOrigin = vec3(0.0, 0.0, 4.0);
  float focalLength = 2.0;
  vec3 rayDirection = normalize(vec3(uv, -focalLength));

  vec3 color = mix(vec3(0.16, 0.18, 0.22), vec3(0.05, 0.06, 0.08), 0.5 * uv.y + 0.5); // background

  float t = march(rayOrigin, rayDirection);
  if (t > 0.0) {
    vec3 normal = calcNormal(rayOrigin + rayDirection * t);
    color = 0.5 * normal + 0.5; // show the normal as a color
  }

  fragColor = vec4(color, 1.0);
}
