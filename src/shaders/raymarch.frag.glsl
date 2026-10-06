// Ray marching (sphere tracing) of a signed distance function.
//
// A signed distance function (SDF) returns, for any point p in space, the distance
// from p to the nearest surface: positive outside, negative inside, zero on the surface.
// The scene is not a list of triangles - it is one function, map(p), defined in scene.glsl.

uniform vec2 uResolution;
uniform int uView; // 0 overview, 1 close-up of the countertop edge

out vec4 fragColor;

const int MAX_STEPS = 200;
const float MAX_DISTANCE = 30.0;
const float SURFACE_EPSILON = 0.0005;

// Walk along the ray. At each point the SDF tells us how far we can safely step
// without passing through any surface, so we step exactly that far.
// Returns vec2(distance travelled, material), or a negative distance if nothing was hit.
vec2 march(vec3 rayOrigin, vec3 rayDirection) {
  float t = 0.0;
  for (int i = 0; i < MAX_STEPS; i++) {
    vec2 hit = map(rayOrigin + rayDirection * t);
    if (hit.x < SURFACE_EPSILON * t) return vec2(t, hit.y);
    t += hit.x;
    if (t > MAX_DISTANCE) break;
  }
  return vec2(-1.0);
}

// The normal is the direction in which the distance grows fastest: the gradient of
// the SDF, estimated with central differences.
vec3 calcNormal(vec3 p) {
  vec2 e = vec2(0.0005, 0.0);
  return normalize(vec3(
    map(p + e.xyy).x - map(p - e.xyy).x,
    map(p + e.yxy).x - map(p - e.yxy).x,
    map(p + e.yyx).x - map(p - e.yyx).x
  ));
}

// A plain color per material. Textures and real lighting come in later parts.
vec3 materialColor(float material) {
  if (material == MATERIAL_FLOOR) return vec3(0.50, 0.38, 0.27);
  if (material == MATERIAL_WALL) return vec3(0.88, 0.86, 0.82);
  if (material == MATERIAL_CABINET) return vec3(0.20, 0.27, 0.33);
  if (material == MATERIAL_STONE) return vec3(0.80, 0.80, 0.78);
  return vec3(0.72, 0.38, 0.26); // vase
}

// Camera: build the ray through this pixel for a camera at `eye` looking at `target`.
// forward/right/up are the camera's coordinate frame, like the view matrix in the homework.
vec3 cameraRay(vec2 uv, vec3 eye, vec3 target, float focalLength) {
  vec3 forward = normalize(target - eye);
  vec3 right = normalize(cross(forward, vec3(0.0, 1.0, 0.0)));
  vec3 up = cross(right, forward);
  return normalize(uv.x * right + uv.y * up + focalLength * forward);
}

void main() {
  // Pixel -> point on the image plane, with (0,0) in the centre and y in [-1, 1].
  vec2 uv = (2.0 * gl_FragCoord.xy - uResolution) / uResolution.y;

  vec3 eye = vec3(1.45, 1.42, 1.50);
  vec3 target = vec3(0.0, 0.80, -0.50);
  float focalLength = 1.9;
  if (uView == 1) {
    // Close to the front right corner, where the edge profile is seen in cross-section.
    eye = vec3(1.46, 0.95, -0.02);
    target = vec3(1.05, 0.87, -0.30);
    focalLength = 2.3;
  }
  vec3 rayDirection = cameraRay(uv, eye, target, focalLength);

  vec3 color = vec3(0.60, 0.68, 0.78); // sky, only visible if a ray hits nothing

  vec2 hit = march(eye, rayDirection);
  if (hit.x > 0.0) {
    vec3 normal = calcNormal(eye + rayDirection * hit.x);
    // Temporary shading so the shapes can be told apart: a fixed light direction
    // and a constant ambient term. Part 4 replaces this with the real lighting.
    vec3 lightDirection = normalize(vec3(0.35, 0.90, 0.50));
    float diffuse = max(dot(normal, lightDirection), 0.0);
    color = materialColor(hit.y) * (0.35 + 0.65 * diffuse);
  }

  fragColor = vec4(color, 1.0);
}
