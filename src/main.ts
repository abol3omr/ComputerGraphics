import GUI from "lil-gui";
import { CABINET_HEIGHT, OrbitCamera, WALL_Z, type Vec3 } from "./camera";
import { createProgram, resizeToDisplay } from "./gl";
import { sunAt } from "./sun";
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
const uEye = uniform("uEye");
const uTarget = uniform("uTarget");
const uFocalLength = uniform("uFocalLength");
const uLightDirection = uniform("uLightDirection");
const uLa = uniform("La");
const uLd = uniform("Ld");
const uLs = uniform("Ls");
const uLength = uniform("uLength");
const uDepth = uniform("uDepth");
const uThickness = uniform("uThickness");
const uStone = uniform("uStone");
const uSlab = uniform("uSlab");
const uPolish = uniform("uPolish");
const uReflections = uniform("uReflections");
const uFloor = uniform("uFloor");
const uWallColor = uniform("uWallColor");
const uCabinetColor = uniform("uCabinetColor");
const uShadows = uniform("uShadows");
const uAmbientOcclusion = uniform("uAmbientOcclusion");

// --- settings: everything the customer can choose ---------------------------
const EDGES = ["Square", "Bevelled", "Rounded", "Bullnose"] as const;
const VIEWS = ["Overview", "Edge close-up"] as const;
const STONES = ["White marble", "Black marble", "Green marble", "Travertine"] as const;
const FLOORS = ["Wood planks", "Tiles", "Concrete"] as const;
// The finish and how polished it is: 1 = mirror-like, 0 = fully matte.
const FINISHES = { Polished: 1.0, Satin: 0.45, Matte: 0.0 } as const;
type Finish = keyof typeof FINISHES;

const settings = {
  stone: "White marble" as (typeof STONES)[number],
  slab: 0, // which part of the stone the slab is cut from
  finish: "Polished" as Finish,
  edge: "Rounded" as (typeof EDGES)[number],
  length: 220, // cm, along the wall
  depth: 63, // cm, from the wall to the front edge
  thickness: 4, // cm
  floor: "Wood planks" as (typeof FLOORS)[number],
  wallColor: "#d3d0ca",
  cabinetColor: "#5a6f7d",
  time: 9.5, // hour of the day, 6 = sunrise, 18 = sunset
  shadows: true,
  ambientOcclusion: true,
  reflections: true,
};

// Settings can also be given in the address bar, e.g. ?edge=Bullnose&length=300&thickness=2
// (handy for reproducing a picture from the report). ?ui=0 hides the control panel.
const query = new URLSearchParams(location.search);
const queryEdge = query.get("edge") as (typeof EDGES)[number] | null;
const queryView = query.get("view") as (typeof VIEWS)[number] | null;
if (queryEdge && EDGES.includes(queryEdge)) settings.edge = queryEdge;
const queryStone = query.get("stone") as (typeof STONES)[number] | null;
if (queryStone && STONES.includes(queryStone)) settings.stone = queryStone;
for (const key of ["length", "depth", "thickness", "slab", "time"] as const) {
  const value = Number(query.get(key));
  if (query.has(key) && Number.isFinite(value)) settings[key] = value;
}
const queryFinish = query.get("finish") as Finish | null;
if (queryFinish && queryFinish in FINISHES) settings.finish = queryFinish;
const queryFloor = query.get("floor") as (typeof FLOORS)[number] | null;
if (queryFloor && FLOORS.includes(queryFloor)) settings.floor = queryFloor;
for (const key of ["wallColor", "cabinetColor"] as const) {
  const value = query.get(key); // six hex digits, without the #
  if (value && /^[0-9a-fA-F]{6}$/.test(value)) settings[key] = "#" + value;
}
for (const key of ["shadows", "ambientOcclusion", "reflections"] as const) {
  if (query.has(key)) settings[key] = query.get(key) !== "0";
}

/**
 * Convert a "#rrggbb" color from the color picker to linear RGB.
 * The picker shows gamma-encoded (sRGB) colors, but the lighting is computed in linear
 * light and gamma-corrected at the end, so the color has to be decoded first.
 */
function hexToLinear(hex: string): [number, number, number] {
  const n = parseInt(hex.slice(1), 16);
  const channel = (c: number) => Math.pow(c / 255, 2.2);
  return [channel((n >> 16) & 255), channel((n >> 8) & 255), channel(n & 255)];
}

// --- camera -------------------------------------------------------------------
const camera = new OrbitCamera();

/** The two starting views. After choosing one, the camera can be moved freely. */
function setView(view: (typeof VIEWS)[number]): void {
  if (view === "Overview") {
    // Step back further for a longer countertop so it stays in the picture.
    const target: Vec3 = [0, 0.8, -0.5];
    const back = Math.max(1, 0.25 + (0.75 * settings.length) / 220);
    camera.lookFrom([target[0] + 1.45 * back, target[1] + 0.62 * back, target[2] + 2.0 * back], target, 1.9);
  } else {
    // Close to the front right corner of the slab, where the edge profile is seen in
    // cross-section.
    const corner: Vec3 = [
      settings.length / 200,
      CABINET_HEIGHT + settings.thickness / 100,
      WALL_Z + settings.depth / 100,
    ];
    const target: Vec3 = [corner[0] - 0.08, corner[1] - 0.03, corner[2] - 0.03];
    camera.lookFrom([corner[0] + 0.33, corner[1] + 0.05, corner[2] + 0.25], target, 2.3);
  }
  draw();
}

// While the camera is moving or the day is playing, render at half resolution so the
// picture keeps up, then redraw at full resolution when things come to rest.
let moving = false;

// Coalesce redraws: however many things change, draw at most once per frame.
let drawScheduled = false;
function draw(): void {
  if (drawScheduled) return;
  drawScheduled = true;
  requestAnimationFrame(() => {
    drawScheduled = false;
    render();
  });
}

// A control in the panel changed. Sliders and color pickers fire many times per second
// while they are being dragged, so they get the same treatment as the camera: half
// resolution while the value is changing, and one full-resolution picture when it stops.
let settleTimer = 0;
function changed(): void {
  moving = true;
  draw();
  window.clearTimeout(settleTimer);
  settleTimer = window.setTimeout(() => {
    moving = false;
    draw();
  }, 250);
}

function render(): void {
  resizeToDisplay(canvas, moving ? 0.5 : 1);
  gl!.viewport(0, 0, canvas.width, canvas.height);
  gl!.useProgram(program);
  gl!.uniform2f(uResolution, canvas.width, canvas.height);
  gl!.uniform1i(uEdge, EDGES.indexOf(settings.edge));
  gl!.uniform3fv(uEye, camera.eye());
  gl!.uniform3fv(uTarget, camera.target);
  gl!.uniform1f(uFocalLength, camera.focalLength);
  const sun = sunAt(settings.time);
  gl!.uniform3fv(uLightDirection, sun.direction);
  gl!.uniform3fv(uLa, sun.La);
  gl!.uniform3fv(uLd, sun.Ld);
  gl!.uniform3fv(uLs, sun.Ld); // the specular light is the same light
  // The panel shows centimetres; the scene is in metres.
  gl!.uniform1f(uLength, settings.length / 100);
  gl!.uniform1f(uDepth, settings.depth / 100);
  gl!.uniform1f(uThickness, settings.thickness / 100);
  gl!.uniform1i(uStone, STONES.indexOf(settings.stone));
  gl!.uniform1f(uSlab, settings.slab);
  gl!.uniform1f(uPolish, FINISHES[settings.finish]);
  gl!.uniform1f(uReflections, settings.reflections ? 1 : 0);
  gl!.uniform1i(uFloor, FLOORS.indexOf(settings.floor));
  gl!.uniform3fv(uWallColor, hexToLinear(settings.wallColor));
  gl!.uniform3fv(uCabinetColor, hexToLinear(settings.cabinetColor));
  gl!.uniform1f(uShadows, settings.shadows ? 1 : 0);
  gl!.uniform1f(uAmbientOcclusion, settings.ambientOcclusion ? 1 : 0);
  gl!.drawArrays(gl!.TRIANGLES, 0, 3); // the full-screen triangle
}

// --- control panel -----------------------------------------------------------
const gui = new GUI({ title: "Stone Previewer" });
gui.add(settings, "stone", [...STONES]).onChange(changed);
gui.add(settings, "slab", 0, 20, 1).name("slab number").onChange(changed);
gui.add(settings, "finish", Object.keys(FINISHES)).onChange(changed);
gui.add(settings, "edge", [...EDGES]).name("edge profile").onChange(changed);

const views = gui.addFolder("View (drag to rotate, scroll to zoom)");
views.add({ overview: () => setView("Overview") }, "overview").name("Overview");
views.add({ closeUp: () => setView("Edge close-up") }, "closeUp").name("Edge close-up");

const size = gui.addFolder("Countertop size (cm)");
size.add(settings, "length", 100, 320, 5).onChange(changed);
size.add(settings, "depth", 50, 90, 1).onChange(changed);
size.add(settings, "thickness", 2, 6, 0.5).onChange(changed);

const room = gui.addFolder("Room");
room.add(settings, "floor", [...FLOORS]).onChange(changed);
room.addColor(settings, "wallColor").name("wall color").onChange(changed);
room.addColor(settings, "cabinetColor").name("cabinet color").onChange(changed);

// --- time of day ---------------------------------------------------------------
// "Play the day" animates the hour from sunrise to sunset: the time is interpolated
// linearly between two keys (6:00 and 18:00) over a fixed duration.
const DAY_DURATION_MS = 12000;
let playStart = 0;

function playDay(): void {
  playStart = performance.now();
  moving = true;
  const step = (now: number) => {
    const t = Math.min(1, (now - playStart) / DAY_DURATION_MS);
    settings.time = Math.round((6 + 12 * t) * 100) / 100;
    timeController.updateDisplay();
    if (t >= 1) moving = false;
    render();
    if (t < 1) requestAnimationFrame(step);
  };
  requestAnimationFrame(step);
}

const light = gui.addFolder("Lighting");
const timeController = light.add(settings, "time", 6, 18, 0.1).name("time of day").onChange(changed);
light.add({ playDay }, "playDay").name("Play the day");
light.add(settings, "shadows").name("soft shadows").onChange(changed);
light.add(settings, "ambientOcclusion").name("ambient occlusion").onChange(changed);
light.add(settings, "reflections").onChange(changed);
if (query.get("ui") === "0") gui.hide();

camera.attach(
  canvas,
  () => {
    moving = true;
    draw();
  },
  () => {
    moving = false;
    draw();
  },
);

window.addEventListener("resize", draw);
setView(queryView && VIEWS.includes(queryView) ? queryView : "Overview");

// Optional exact camera angles in the address bar, in degrees and metres:
// ?azimuth=-40&elevation=30&distance=3
if (query.has("azimuth")) camera.azimuth = (Number(query.get("azimuth")) * Math.PI) / 180;
if (query.has("elevation")) camera.elevation = (Number(query.get("elevation")) * Math.PI) / 180;
if (query.has("distance")) camera.distance = Number(query.get("distance"));
