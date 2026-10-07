// Small WebGL2 helpers: compile a shader program and draw one triangle that covers the screen.

const HEADER = "#version 300 es\nprecision highp float;\nprecision highp int;\n";

export function createProgram(
  gl: WebGL2RenderingContext,
  vertexSource: string,
  fragmentSource: string,
): WebGLProgram {
  const compile = (type: number, source: string): WebGLShader => {
    const shader = gl.createShader(type)!;
    gl.shaderSource(shader, HEADER + source);
    gl.compileShader(shader);
    if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
      throw new Error("Shader compile error:\n" + gl.getShaderInfoLog(shader));
    }
    return shader;
  };

  const program = gl.createProgram()!;
  gl.attachShader(program, compile(gl.VERTEX_SHADER, vertexSource));
  gl.attachShader(program, compile(gl.FRAGMENT_SHADER, fragmentSource));
  gl.linkProgram(program);
  if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
    throw new Error("Program link error:\n" + gl.getProgramInfoLog(program));
  }
  return program;
}

/**
 * Make the drawing buffer match the size of the canvas on screen.
 * `scale` below 1 renders fewer pixels (the browser stretches the result), which keeps
 * the picture responsive while the camera is moving: the cost of ray marching is
 * proportional to the number of pixels.
 */
export function resizeToDisplay(canvas: HTMLCanvasElement, scale = 1): void {
  const dpr = Math.min(window.devicePixelRatio || 1, 2) * scale;
  const width = Math.max(1, Math.round(canvas.clientWidth * dpr));
  const height = Math.max(1, Math.round(canvas.clientHeight * dpr));
  if (canvas.width !== width || canvas.height !== height) {
    canvas.width = width;
    canvas.height = height;
  }
}
