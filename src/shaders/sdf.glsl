// Signed distance functions for basic shapes, and ways to combine them.
// Each returns the distance from point p to the surface of the shape.

float sdSphere(vec3 p, float radius) {
  return length(p) - radius;
}

// Box centred on the origin; halfSize is half of its width, height and depth.
float sdBox(vec3 p, vec3 halfSize) {
  vec3 q = abs(p) - halfSize;
  return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

// Union of two shapes is simply the smaller of the two distances: min(a, b).
// The smooth minimum blends the two surfaces together over a distance k,
// so the shapes melt into each other instead of meeting at a sharp crease.
float smoothMin(float a, float b, float k) {
  float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
  return mix(b, a, h) - k * h * (1.0 - h);
}
