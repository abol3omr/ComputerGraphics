// One big triangle that covers the whole screen. There is no geometry in this
// project: the vertex shader only makes sure the fragment shader runs for every pixel.
void main() {
  vec2 corner = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2); // (0,0) (2,0) (0,2)
  gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
}
