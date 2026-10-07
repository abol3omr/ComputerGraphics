// Ray marching (sphere tracing) of a signed distance function.
//
// A signed distance function (SDF) returns, for any point p in space, the distance
// from p to the nearest surface: positive outside, negative inside, zero on the surface.
// The scene is not a list of triangles - it is one function, map(p), defined in scene.glsl.
// Lighting is in lighting.glsl and the surface properties are in materials.glsl.

uniform vec2 uResolution;
uniform vec3 uEye;          // camera position (from the orbit camera in camera.ts)
uniform vec3 uTarget;       // the point the camera looks at
uniform float uFocalLength; // larger = narrower field of view
uniform float uReflections; // 1 = on, 0 = off

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

// Camera: build the ray through this pixel for a camera at `eye` looking at `target`.
// forward/right/up are the camera's coordinate frame, like the view matrix in the homework.
vec3 cameraRay(vec2 uv, vec3 eye, vec3 target, float focalLength) {
  vec3 forward = normalize(target - eye);
  vec3 right = normalize(cross(forward, vec3(0.0, 1.0, 0.0)));
  vec3 up = cross(right, forward);
  return normalize(uv.x * right + uv.y * up + focalLength * forward);
}

// What a ray sees when it hits nothing. Only the wall behind the countertop is modelled,
// so this stands in for the other walls and the ceiling, which matter mainly in
// reflections. They take the chosen wall color, lit roughly like the real wall, and the
// ceiling is a lighter version of it.
vec3 environment(vec3 direction) {
  vec3 light = La + 0.55 * Ld;
  vec3 walls = uWallColor * light;
  vec3 ceiling = mix(uWallColor, vec3(0.85), 0.6) * light;
  return mix(walls, ceiling, smoothstep(0.3, 0.8, direction.y));
}

// Follow one ray into the scene and return the color it sees.
// If it hits a surface, `p`, `n` and `material` describe the hit point; otherwise hit = false.
vec3 trace(vec3 rayOrigin, vec3 rayDirection, out bool hit, out vec3 p, out vec3 n, out Material material) {
  vec2 result = march(rayOrigin, rayDirection);
  hit = result.x > 0.0;
  if (!hit) return environment(rayDirection);
  p = rayOrigin + rayDirection * result.x;
  n = calcNormal(p);
  material = getMaterial(result.y, p);
  return phongReflection(p, n, -rayDirection, material); // v = direction back to the eye
}

void main() {
  // Pixel -> point on the image plane, with (0,0) in the centre and y in [-1, 1].
  vec2 uv = (2.0 * gl_FragCoord.xy - uResolution) / uResolution.y;

  vec3 eye = uEye;
  vec3 rayDirection = cameraRay(uv, eye, uTarget, uFocalLength);

  bool hit;
  vec3 p, n;
  Material material;
  vec3 color = trace(eye, rayDirection, hit, p, n, material);

  // Mirror reflection: if the surface is reflective, send a second ray in the mirror
  // direction and blend in what it sees. This is one bounce of ray tracing.
  if (hit && material.reflectivity > 0.0 && uReflections > 0.5) {
    vec3 mirrorDirection = reflect(rayDirection, n);

    // Fresnel effect (Schlick's approximation): a surface reflects more at a grazing
    // angle than when you look straight at it. cosTheta is 1 looking straight on.
    float cosTheta = max(dot(n, -rayDirection), 0.0);
    // A less polished surface also reflects less at grazing angles, so the upper limit
    // is tied to the reflectivity instead of being 1.
    float grazing = min(1.0, 8.0 * material.reflectivity);
    float fresnel = material.reflectivity + (grazing - material.reflectivity) * pow(1.0 - cosTheta, 5.0);

    bool hit2;
    vec3 p2, n2;
    Material material2;
    vec3 reflected = trace(p + n * 0.002, mirrorDirection, hit2, p2, n2, material2);
    color = mix(color, reflected, fresnel);
  }

  // "Beware of overflows": clamp, then gamma-correct, because the lighting is computed
  // in linear light but the screen expects gamma-encoded values.
  color = pow(clamp(color, 0.0, 1.0), vec3(1.0 / 2.2));
  fragColor = vec4(color, 1.0);
}
