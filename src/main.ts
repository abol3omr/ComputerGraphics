import GUI from "lil-gui";
import { createProgram, resizeToDisplay } from "./gl";
import vertexSource from "./shaders/fullscreen.vert.glsl?raw";
import sdf from "./shaders/sdf.glsl?raw";
import scene from "./shaders/scene.glsl?raw";
import raymarch from "./shaders/raymarch.frag.glsl?raw";

// The fragment shader is assembled from three files: shapes, the scene, and the ray marcher.
const fragmentSource = sdf + scene + raymarch;

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

// --- settings: everything the customer can choose ---------------------------
const EDGES = ["Square", "Bevelled", "Rounded", "Bullnose"] as const;
const VIEWS = ["Overview", "Edge close-up"] as const;

const settings = {
  edge: "Rounded" as (typeof EDGES)[number],
  view: "Overview" as (typeof VIEWS)[number],
  length: 220, // cm, along the wall
  depth: 63, // cm, from the wall to the front edge
  thickness: 4, // cm
};

// Settings can also be given in the address bar, e.g. ?edge=Bullnose&length=300&thickness=2
// (handy for reproducing a picture from the report). ?ui=0 hides the control panel.
const query = new URLSearchParams(location.search);
const queryEdge = query.get("edge") as (typeof EDGES)[number] | null;
const queryView = query.get("view") as (typeof VIEWS)[number] | null;
if (queryEdge && EDGES.includes(queryEdge)) settings.edge = queryEdge;
if (queryView && VIEWS.includes(queryView)) settings.view = queryView;
for (const key of ["length", "depth", "thickness"] as const) {
  const value = Number(query.get(key));
  if (query.has(key) && Number.isFinite(value)) settings[key] = value;
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
  gl!.drawArrays(gl!.TRIANGLES, 0, 3); // the full-screen triangle
}

// --- control panel -----------------------------------------------------------
const gui = new GUI({ title: "Stone Previewer" });
gui.add(settings, "edge", [...EDGES]).name("edge profile").onChange(draw);
gui.add(settings, "view", [...VIEWS]).onChange(draw);

const size = gui.addFolder("Countertop size (cm)");
size.add(settings, "length", 100, 320, 5).onChange(draw);
size.add(settings, "depth", 50, 90, 1).onChange(draw);
size.add(settings, "thickness", 2, 6, 0.5).onChange(draw);
if (query.get("ui") === "0") gui.hide();

window.addEventListener("resize", draw);
draw();
