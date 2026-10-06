# Stone Previewer - Project Report

**Idea.** Customers choosing a stone countertop have to decide on an edge profile and a finish, and it
is hard to imagine either from a small sample. This app shows the countertop in a room and lets the
customer switch the stone, the edge and the finish, and move the light from morning to evening.

**Technique.** The homework engine is a rasterizer: it loads a triangle mesh, projects it, fills the
triangles and lights them locally. This project uses the opposite approach. There is no mesh. The scene
is a mathematical function, and each pixel shoots a ray into it (ray marching). That makes rounded
edges, soft shadows and reflections cheap, which are exactly the things this app needs to show.

## Part 1: Ray Marching a Sphere

### Approach

- The app draws a single triangle that covers the whole screen, so the fragment shader runs once for
  every pixel. All the work happens in that shader.
- The scene is described by a **signed distance function** (SDF), `map(p)`: for any point `p` it returns
  the distance to the nearest surface (positive outside, negative inside). For a sphere of radius `r`
  centred on the origin this is simply `length(p) - r`.
- For each pixel I build a ray from the camera through that pixel and **march** along it: at every
  point the SDF says how far the nearest surface is, so the ray can safely step exactly that far
  without passing through anything. The loop stops when the distance is below a small epsilon (hit) or
  the ray has gone too far (miss).
- The surface normal is the gradient of the SDF, which I estimate with central differences
  (six extra calls to `map`). In the homework the normal came from the cross product of triangle edges;
  here there are no triangles to take it from.
- To check the result, the normal is shown directly as a color: `color = 0.5 * normal + 0.5`.

### Result

The sphere is smooth at any zoom level because it is evaluated per pixel, not approximated by polygons.
The colors confirm the normals: right is red (+x), up is green (+y), and facing the camera is blue (+z).

![A ray-marched sphere with its normals shown as colors](./assets/part1_sphere.png)
