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
const uResolution = gl.getUniformLocation(program, "uResolution");

function draw(): void {
  resizeToDisplay(canvas);
  gl!.viewport(0, 0, canvas.width, canvas.height);
  gl!.useProgram(program);
  gl!.uniform2f(uResolution, canvas.width, canvas.height);
  gl!.drawArrays(gl!.TRIANGLES, 0, 3); // the full-screen triangle
}

window.addEventListener("resize", draw);
draw();
