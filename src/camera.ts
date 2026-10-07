// Orbit camera: the eye moves on a sphere around a target point.
// Drag to rotate, scroll to zoom. The shader only receives the resulting eye and target.

export type Vec3 = [number, number, number];

// These two must match the constants in scene.glsl.
export const WALL_Z = -0.9;
export const CABINET_HEIGHT = 0.86;

export class OrbitCamera {
  target: Vec3 = [0, 0.8, -0.5];
  /** Angle around the vertical axis, in radians. 0 = straight in front of the wall. */
  azimuth = 0.6;
  /** Angle above the horizontal, in radians. */
  elevation = 0.25;
  distance = 2.5;
  focalLength = 1.9;

  /** Spherical coordinates -> position of the eye. */
  eye(): Vec3 {
    const horizontal = this.distance * Math.cos(this.elevation);
    return [
      this.target[0] + horizontal * Math.sin(this.azimuth),
      this.target[1] + this.distance * Math.sin(this.elevation),
      this.target[2] + horizontal * Math.cos(this.azimuth),
    ];
  }

  /** Place the camera at `eye` looking at `target` (the inverse of eye()). */
  lookFrom(eye: Vec3, target: Vec3, focalLength: number): void {
    const d: Vec3 = [eye[0] - target[0], eye[1] - target[1], eye[2] - target[2]];
    this.target = target;
    this.distance = Math.hypot(d[0], d[1], d[2]);
    this.elevation = Math.asin(d[1] / this.distance);
    this.azimuth = Math.atan2(d[0], d[2]);
    this.focalLength = focalLength;
    this.clamp();
  }

  /** Keep the eye in front of the wall, above the floor and at a sensible distance. */
  private clamp(): void {
    const clampTo = (x: number, lo: number, hi: number) => Math.min(hi, Math.max(lo, x));
    this.distance = clampTo(this.distance, 0.35, 9);
    this.azimuth = clampTo(this.azimuth, -1.35, 1.35);
    // Lowest elevation that still keeps the eye 10 cm above the floor.
    const lowest = Math.asin(clampTo((0.1 - this.target[1]) / this.distance, -1, 1));
    this.elevation = clampTo(this.elevation, Math.max(lowest, -0.3), 1.45);
  }

  /**
   * Connect mouse / touch input.
   * onChange is called while the camera moves, onRest once the movement has stopped.
   */
  attach(canvas: HTMLCanvasElement, onChange: () => void, onRest: () => void): void {
    let restTimer = 0;

    canvas.addEventListener("pointerdown", (e) => canvas.setPointerCapture(e.pointerId));
    canvas.addEventListener("pointermove", (e) => {
      if (e.buttons === 0) return;
      this.azimuth -= e.movementX * 0.006;
      this.elevation += e.movementY * 0.006;
      this.clamp();
      onChange();
    });
    canvas.addEventListener("pointerup", onRest);

    canvas.addEventListener(
      "wheel",
      (e) => {
        e.preventDefault();
        this.distance *= Math.exp(e.deltaY * 0.001);
        this.clamp();
        onChange();
        window.clearTimeout(restTimer);
        restTimer = window.setTimeout(onRest, 200);
      },
      { passive: false },
    );
  }
}
