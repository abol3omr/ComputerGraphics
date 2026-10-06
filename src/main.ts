import GUI from "lil-gui";
import { createProgram, resizeToDisplay } from "./gl";
import vertexSource from "./shaders/fullscreen.vert.glsl?raw";
import sdf from "./shaders/sdf.glsl?raw";
import scene from "./shaders/scene.glsl?raw";
import noise from "./shaders/noise.glsl?raw";
import materials from "./shaders/materials.glsl?raw";
import lighting from "./shaders/lighting.glsl?raw";
import raymarch from "./shaders/raymarch.frag.glsl?raw";

// The fragment shader is assembled from several files, in dependency order:
// shapes, the scene, noise, materials, lighting, and finally the ray marcher with main().
const fragmentSource = sdf + scene + noise + materials + lighting + raymarch;

const canvas = document.querySelector<HTMLCanvasElement>("#view")!;
const gl = canvas.getContext("webgl2");
if (!gl) throw new Error("WebGL2 is not available in this browser");

const program = createProgram(gl, vertexSource, fragmentSource);
const uniform = (name: string) => gl.getUniformLocation(program, name);
const uResolution = uniform("uResolution");
const uEdge = uniform("uEdge");
const uView = uniform("uView");
const uLength = uniform("uLength");
const uDepth = uniform("uDepth");
const uThickness = uniform("uThickness");
const uStone = uniform("uStone");
const uSlab = uniform("uSlab");
const uShadows = uniform("uShadows");
const uAmbientOcclusion = uniform("uAmbientOcclusion");

// --- settings: everything the customer can choose ---------------------------
const EDGES = ["Square", "Bevelled", "Rounded", "Bullnose"] as const;
const VIEWS = ["Overview", "Edge close-up"] as const;
const STONES = ["White marble", "Black marble", "Green marble", "Travertine"] as const;

const settings = {
  stone: "White marble" as (typeof STONES)[number],
  slab: 0, // which part of the stone the slab is cut from
  edge: "Rounded" as (typeof EDGES)[number],
  view: "Overview" as (typeof VIEWS)[number],
  length: 220, // cm, along the wall
  depth: 63, // cm, from the wall to the front edge
  thickness: 4, // cm
  shadows: true,
  ambientOcclusion: true,
};

// Settings can also be given in the address bar, e.g. ?edge=Bullnose&length=300&thickness=2
// (handy for reproducing a picture from the report). ?ui=0 hides the control panel.
const query = new URLSearchParams(location.search);
const queryEdge = query.get("edge") as (typeof EDGES)[number] | null;
const queryView = query.get("view") as (typeof VIEWS)[number] | null;
if (queryEdge && EDGES.includes(queryEdge)) settings.edge = queryEdge;
if (queryView && VIEWS.includes(queryView)) settings.view = queryView;
const queryStone = query.get("stone") as (typeof STONES)[number] | null;
if (queryStone && STONES.includes(queryStone)) settings.stone = queryStone;
for (const key of ["length", "depth", "thickness", "slab"] as const) {
  const value = Number(query.get(key));
  if (query.has(key) && Number.isFinite(value)) settings[key] = value;
}
for (const key of ["shadows", "ambientOcclusion"] as const) {
  if (query.has(key)) settings[key] = query.get(key) !== "0";
}

function draw(): void {
  resizeToDisplay(canvas);
  gl!.viewport(0, 0, canvas.width, canvas.height);
  gl!.useProgram(program);
  gl!.uniform2f(uResolution, canvas.width, canvas.height);
  gl!.uniform1i(uEdge, EDGES.indexOf(settings.edge));
  gl!.uniform1i(uView, VIEWS.indexOf(settings.view));
  // The panel shows centimetres; the scene is in metres.
  gl!.uniform1f(uLength, settings.length / 100);
  gl!.uniform1f(uDepth, settings.depth / 100);
  gl!.uniform1f(uThickness, settings.thickness / 100);
  gl!.uniform1i(uStone, STONES.indexOf(settings.stone));
  gl!.uniform1f(uSlab, settings.slab);
  gl!.uniform1f(uShadows, settings.shadows ? 1 : 0);
  gl!.uniform1f(uAmbientOcclusion, settings.ambientOcclusion ? 1 : 0);
  gl!.drawArrays(gl!.TRIANGLES, 0, 3); // the full-screen triangle
}

// --- control panel -----------------------------------------------------------
const gui = new GUI({ title: "Stone Previewer" });
gui.add(settings, "stone", [...STONES]).onChange(draw);
gui.add(settings, "slab", 0, 20, 1).name("slab number").onChange(draw);
gui.add(settings, "edge", [...EDGES]).name("edge profile").onChange(draw);
gui.add(settings, "view", [...VIEWS]).onChange(draw);

const size = gui.addFolder("Countertop size (cm)");
size.add(settings, "length", 100, 320, 5).onChange(draw);
size.add(settings, "depth", 50, 90, 1).onChange(draw);
size.add(settings, "thickness", 2, 6, 0.5).onChange(draw);

const light = gui.addFolder("Lighting");
light.add(settings, "shadows").name("soft shadows").onChange(draw);
light.add(settings, "ambientOcclusion").name("ambient occlusion").onChange(draw);
if (query.get("ui") === "0") gui.hide();

window.addEventListener("resize", draw);
draw();
