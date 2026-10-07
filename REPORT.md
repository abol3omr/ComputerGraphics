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

## Part 2: Building the Room from Distance Functions

### Approach

- I added a second primitive, the box (`sdBox`), next to the sphere. Planes are even simpler: the floor
  is `p.y` and the wall is `p.z - WALL_Z`.
- The scene function `map(p)` now combines several shapes. The **union** of two shapes is the minimum
  of their distances, so the room is built by taking the closest of: floor, wall, cabinet, countertop
  slab, backsplash and a vase. All sizes are in metres (the cabinet is 0.86 m high, the slab 4 cm thick
  with a 3 cm overhang), so the proportions are those of a real kitchen unit.
- `map` returns two values: the distance and a **material id** of the closest shape. The ray marcher
  passes the id on, and the shader picks a color from it. This replaces the per-face material of a mesh.
- The vase is two spheres joined with a **smooth minimum** instead of `min`. It blends the two surfaces
  over a small distance, so they melt into one shape with no crease. With a mesh this would need
  remodelling; here it is one line.
- The camera is no longer fixed on the z axis. `cameraRay` builds a forward / right / up frame from an
  eye position and a target, the same idea as the view matrix in the homework, but used to aim rays
  instead of transforming vertices.
- The shading is still temporary: one fixed light direction plus a constant ambient term, only so the
  shapes can be told apart. There are no shadows yet, which is why the scene looks flat.

### Result

The whole room is about 30 lines of distance functions and contains no triangles. Note the vase: its
body and neck join smoothly.

![The room: floor, wall, cabinet, countertop, backsplash and vase with plain colors](./assets/part2_room.png)

## Part 3: Edge Profiles

### Approach

- This is the first feature the customer actually uses: choosing between a **square**, **bevelled**,
  **rounded** and **bullnose** edge.
- I design the edge in 2D, as the cross-section of the slab seen from the side, and then extrude that
  shape along the length of the countertop. The countertop is no longer `sdBox` but its own function,
  `sdSlab`.
- All the profiles come from one formula, the 2D rounded rectangle, by changing a single number, the
  corner radius: 0 gives the square edge, 10 mm gives the rounded edge, and half the slab thickness
  turns the front into a half circle, which is the bullnose.
- The bevelled edge uses the **intersection** of two shapes, which for distance functions is
  `max(a, b)`: the square profile intersected with a 45 degree plane that cuts 14 mm off the top front
  corner.
- The extrusion is also an intersection: the 2D profile (which is infinitely long in x) is cut to the
  slab's width and at the wall with `max`.
- The choice is sent to the shader as a uniform, `uEdge`, from a small control panel
  ([lil-gui](https://lil-gui.georgealways.com), open source). I also added a second camera position,
  a close-up of the front corner, where the profile is visible in cross-section.

### Result

Switching the edge changes one number in a formula, and the picture updates immediately. On a triangle
mesh, each of these would be a separately modelled object, and the round ones would need many small
polygons to look smooth.

![The four edge profiles seen from the front corner: square, bevelled, rounded, bullnose](./assets/part3_edges.png)

**Limitation:** only the front edge is profiled. The two side ends of the slab stay square, and the
round profiles are rounded on the bottom as well as the top.

## Part 4: Adjustable Countertop Size

### Approach

- Every kitchen is a different size, so the length, depth and thickness of the countertop are now
  sliders (in centimetres) instead of constants.
- In the shader the three constants became uniforms: `uLength`, `uDepth`, `uThickness`. Everything
  else is derived from them inside `map`: the cabinet is the slab minus the overhang, the backsplash
  has the slab's length and thickness (its own edges stay square), and the vase stays at the same relative position on the top.
- The edge profile adapts by itself. The bullnose radius is defined as half the thickness, so a
  thicker slab automatically gets a bigger half circle, and the rounded edge is clamped so its radius
  can never be larger than half the thickness.
- Both cameras follow the size: the overview steps back for a longer countertop, and the close-up is
  defined relative to the front corner of the slab, so it stays on the edge.
- Resizing does not rebuild anything. There is no vertex buffer to regenerate, only three numbers
  that the distance function reads, so the change is immediate.

### Result

The same scene at two sizes. The proportions of the cabinet, backsplash and edge all follow the three
sliders.

![A small 120 x 55 cm countertop next to a large 320 x 85 cm one](./assets/part4_sizes.png)

This matters for the stone texture added later: because the texture will be computed from the 3D
position of each point, a longer countertop will show *more* stone, not a stretched picture of it.

## Part 5: Lighting, Soft Shadows and Ambient Occlusion

### Approach

- I replaced the temporary shading with the **Phong reflection model** from the lecture, using the
  same notation: `I = Ia + Id + Is`, with `Ia = La * ka`, `Id = kd * (l . n) * Ld` and
  `Is = ks * (r . v)^alpha * Ls`. There is one parallel light source, like sun through a window.
- Each material now has its own `k`, `ks` and `alpha` (a `Material` struct in `materials.glsl`), so the
  stone and the vase get a highlight while the wall stays matte.
- Because the normal comes from the distance function at every pixel, this is automatically per-pixel
  (Phong) shading. There are no vertices to interpolate between, so there is no flat / Gouraud / Phong
  choice to make as there was in the homework.
- **Soft shadows.** The homework engine had no shadows, because a rasterizer only knows about the
  triangle it is drawing. Here I march a second ray from the surface point towards the light. If it
  hits something, the point is in shadow. If it only passes *close* to something, the point is in the
  penumbra, and I darken it by how close the miss was (`sharpness * h / t`). This gives a soft edge
  that gets wider further from the object, at no extra cost over a hard shadow.
- **Ambient occlusion.** I sample the distance function at five short steps along the normal. In open
  space the distance there equals the step length. If it is smaller, another surface is nearby and
  blocks part of the ambient light, so I reduce the ambient term. This darkens the corners where the
  cabinet meets the floor and the wall.
- The shadow multiplies only the diffuse and specular terms (direct light), and the occlusion only the
  ambient term (light from everywhere). Both can be switched off in the control panel to compare.
- The final color is clamped ("beware of overflows") and gamma-corrected.

### Result

The same view with the two effects switched on and off. With the reflection model alone, the cabinet
looks like it floats in front of the wall. The shadow places it in the room and shows where the light
comes from, and the ambient occlusion grounds it where it meets the floor and the wall.

![Phong reflection only, with soft shadows, with ambient occlusion, and with both](./assets/part5_lighting.png)

## Part 6: Procedural Stone

### Approach

- The stone is a **procedural texture** built from the functions in the Procedural Textures lecture,
  keeping the names from the slides: `Noise`, `FBm`, `turbulence`, `marble` and `marble_color`.
- `Noise` is classic 3D Perlin (gradient) noise: a pseudo-random gradient at every grid point, taken
  from a hash function instead of a stored table, and Hermite interpolation between the 8 nearest
  grid points.
- `FBm` sums several octaves of the noise, each with double the frequency and half the amplitude.
  `turbulence` is the same sum but of the absolute value of the noise, which creates sharp creases.
- The marble follows the lecture: `x = p.x + turbulence(p)` and `color = marble_color(sin(x))`.
  `sin` alone gives straight parallel stripes, and the turbulence bends them into veins. I changed two
  things: the stripes run along a diagonal direction so the veins cross the countertop at an angle,
  and the palette uses a narrow `smoothstep`, so most of the surface is the base color with thin veins.
- There are four stones (white, black and green marble, and travertine). Each is only five numbers:
  vein color, base color, vein frequency, turbulence amount and vein width. Travertine uses a high
  frequency with little turbulence, which gives its straight bands.
- A **slab number** slider shifts the point before it is textured, which is like cutting the slab
  from a different place in the quarry: the same stone, with a different vein pattern.
- The texture is evaluated on the 3D position of the surface point, in metres. There are no UV
  coordinates and no image.

### Result

![The four stones: white marble, black marble, green marble and travertine](./assets/part6_stones.png)

Two things follow from the texture being a function of the 3D point:

- **It cannot stretch.** Below is the same white marble on a 120 cm and a 320 cm countertop. The veins
  keep their size, and the longer piece simply shows more of the stone. An image texture mapped onto
  a mesh would have to be stretched or tiled.
- **It is continuous across pieces.** The veins run from the countertop up into the backsplash without
  a seam, as if both were carved from one block.

![The same marble at two countertop lengths: the veins do not stretch](./assets/part6_no_stretch.png)

**Limitation:** this is a generic stone *type*, not a photograph of a specific slab, and real veins
are less regular than the ones this function produces.

## Part 7: Room Options

### Approach

- A customer judges a stone against their own kitchen, so the room can now be changed: the floor
  (wood planks, tiles or concrete), the wall color and the cabinet color.
- **Repeating patterns without geometry.** Planks and tiles are not separate objects. The floor is
  still the single plane `p.y = 0`. The pattern comes from the coordinates: `floor(p / size)` says
  *which* plank or tile a point is in, and `fract(p / size)` says *where* inside it. A dark gap or
  grout line is drawn where the point is close to the edge of its cell.
- **A random number per cell.** Hashing the cell index (with the same hash function as the noise)
  gives every plank and tile its own random number. I use it to stagger the rows of planks, to tint
  each plank and tile slightly differently, and to choose where in the log each plank was cut.
- **Wood grain** uses the wood function from the lecture, `x = (p.x^2 + p.y^2) + FBm(p)` and
  `wood_color(sin(x))`. `p.x^2 + p.y^2` is the squared distance from the axis of the log, so `sin` of it
  gives the growth rings, and `FBm` makes them irregular. Each plank evaluates it at a different
  position inside the log, so no two planks have the same grain.
- **Tiles** add a faint `FBm` variation inside each tile and are slightly shiny, but the grout is
  not. **Concrete** is `FBm` for large soft patches plus high-frequency `Noise` for fine grain.
- The **cabinet** color comes from a color picker, and thin gaps between doors are drawn on its front
  face. The number of doors follows the countertop length (one door per 55 cm or so).
- The color picker gives gamma-encoded colors, while the lighting is computed in linear light and
  gamma-corrected at the end. So the picked color is decoded (`c^2.2`) before it is sent to the
  shader; otherwise every picked color would look washed out.

### Result

Four combinations of stone, floor, wall and cabinet. Everything visible is still computed by one
fragment shader, with no image files.

![Four rooms: wood planks, tiles and concrete floors with different cabinet and wall colors](./assets/part7_room_options.png)

**Limitation:** thin lines such as the plank gaps and tile grout flicker a little far from the camera,
because each pixel is sampled only once (there is no anti-aliasing yet).

## Part 8: Polished and Matte Finishes

### Approach

- The second decision the customer makes is the **finish**: polished, satin or matte. The finish does
  not change the color of the stone, only how it reflects light, so it is a single number, `uPolish`,
  between 0 and 1.
- It changes three properties of the stone's material: the specular coefficient `ks` (how strong the
  highlight is), the shininess exponent `alpha` (how small and sharp it is), and a new property,
  `reflectivity` (how mirror-like the surface is).
- **Mirror reflection.** When the camera ray hits a reflective surface, I send a second ray in the
  mirror direction, `reflect(rayDirection, n)`, find what it hits, light that point with the same
  Phong function, and blend the result into the color. This is one bounce of ray tracing. To reuse the
  code, marching, normal, material and lighting are wrapped in one function, `trace`, which is called
  once for the camera ray and once for the reflected ray.
- **Fresnel effect.** A real polished surface reflects more when seen at a grazing angle than when
  seen straight on. I use Schlick's approximation: `F = R0 + (Rmax - R0) * (1 - cos(theta))^5`, where
  `R0` is the material's reflectivity and `theta` is the angle between the view direction and the
  normal.
- The Phong model from the lecture is a *local* illumination model: each point only knows about the
  light, not about other objects. Shadows (Part 5) and this reflection are the two places where the
  project goes beyond it and lets objects affect each other.
- Only the wall behind the countertop is modelled. Rays that leave the scene return the chosen wall
  color (lighter towards the ceiling), which stands in for the other walls and the ceiling. This way
  the reflections in the stone follow the wall color picker. A checkbox switches the reflections off
  for comparison.

### Result

The same black marble with the three finishes. On the polished top the vase and the backsplash are
mirrored, and the highlight is small and sharp. On the matte top there is no mirror image and the
light spreads evenly. The last picture is the polished material with the reflection ray switched
off: the highlight alone does not make it look polished.

![Polished, satin and matte finishes, and polished with reflections switched off](./assets/part8_finishes.png)

**Limitation:** there is only one bounce, and the reflection is always perfectly sharp. A real satin
finish gives a blurred reflection, which would need many rays per pixel.

## Part 9: Orbit Camera and Time of Day

### Approach

- **Orbit camera.** The two fixed views are replaced by a camera the customer can move: drag to
  rotate around the countertop and scroll to zoom. The camera is described by spherical coordinates
  around a target point (azimuth, elevation and distance), and `eye()` converts them to a position.
  The shader receives only the resulting eye and target as uniforms and builds its rays as before.
  The two old views remain as buttons that jump to a starting position.
- The angles are clamped so the eye always stays in front of the wall and above the floor. This is
  necessary here, not only cosmetic: the wall and floor are infinite planes, and a ray that starts
  behind one of them is "inside" the solid and cannot march.
- **Half resolution while moving.** The cost of ray marching is proportional to the number of pixels,
  because every pixel marches its own rays (camera, shadow and reflection). While the camera is being
  dragged, the picture is rendered at half the resolution in each direction, a quarter of the pixels,
  and redrawn at full resolution when the mouse is released. In a rasterizer the cost depends mostly
  on the number of triangles, so this trade-off is specific to this technique.
- **Time of day.** The light is one parallel source, the sun. A slider sets the hour between 6:00 and
  18:00, and `sunAt(hour)` computes the light from it: the direction moves on a half circle from the
  left of the countertop, over the top, to the right, and the color and intensity go from warm and
  weak near the horizon to white and strong at noon. The ambient light changes with it. These values
  are the `l`, `La`, `Ld` and `Ls` of the Phong model, now sent as uniforms.
- **Animation.** "Play the day" animates the hour from sunrise to sunset in 12 seconds. It is the
  simplest kind of keyframe animation from the lecture: two keys on a timeline (6:00 and 18:00) and
  linear interpolation between them, evaluated every frame.

### Result

The same countertop from four camera positions:

![The countertop seen from the left, from above, from the front and from the right](./assets/part9_orbit.png)

Four times of day. The direction of the shadows, their length and the color of the light all change,
which is what lets a customer judge a stone in morning or evening light:

![The scene at 7:00, 9:30, 12:00 and 17:00](./assets/part9_time_of_day.png)

The whole day, as played by the animation:

![Animation of the light from 6:00 to 18:00](./assets/part9_day.gif)

**Limitation:** the sun's path is a simple half circle chosen to look right. It does not depend on
the season, the location or which way the room faces.
