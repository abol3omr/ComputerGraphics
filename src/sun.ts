// The sun: one parallel light source whose direction and color depend on the time of day.

import type { Vec3 } from "./camera";

export interface Sun {
  direction: Vec3; // direction TO the light (not normalized; the shader normalizes it)
  La: Vec3; // ambient light intensity
  Ld: Vec3; // diffuse and specular light intensity
}

const lerp = (a: number, b: number, t: number) => a + (b - a) * t;
const lerp3 = (a: Vec3, b: Vec3, t: number): Vec3 => [lerp(a[0], b[0], t), lerp(a[1], b[1], t), lerp(a[2], b[2], t)];
const smoothstep = (lo: number, hi: number, x: number) => {
  const t = Math.min(1, Math.max(0, (x - lo) / (hi - lo)));
  return t * t * (3 - 2 * t);
};

/**
 * The light at a given hour, from 6 (sunrise) to 18 (sunset).
 *
 * The sun moves on a half circle: it rises on the left of the countertop, is highest at
 * noon, and sets on the right. It always stays in front of the wall, as if the room had
 * a large window behind the viewer.
 */
export function sunAt(hour: number): Sun {
  const angle = ((hour - 6) / 12) * Math.PI; // 0 at sunrise, pi at sunset
  const height = Math.sin(angle); // 0 at the horizon, 1 at noon

  const direction: Vec3 = [-Math.cos(angle), 0.12 + 0.9 * height, 0.6];

  // Low sun is warm and weak, high sun is white and strong.
  const daylight = smoothstep(0.0, 0.6, height);
  const color = lerp3([1.0, 0.55, 0.28], [1.0, 0.96, 0.9], daylight);
  const intensity = 0.55 + 0.32 * smoothstep(0.0, 0.5, height);
  const Ld: Vec3 = [color[0] * intensity, color[1] * intensity, color[2] * intensity];

  // The ambient light (the sky) is dimmer and slightly warmer at the ends of the day.
  const La = lerp3([0.17, 0.15, 0.17], [0.2, 0.22, 0.26], daylight);

  return { direction, La, Ld };
}
