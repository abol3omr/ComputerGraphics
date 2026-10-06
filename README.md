# Stone Previewer

Mini project for the Computer Graphics course (University of Haifa, 2026).

A small web app that helps a customer in a stone shop decide **which edge profile and finish to order**
for a countertop. The customer picks a stone, an edge (square, bevelled, rounded, bullnose) and a finish
(polished or matte), and sees the countertop in a room under morning, noon or evening light.

The scene contains no triangles. It is drawn by **ray marching a signed distance function** in a single
fragment shader, which is the opposite approach to the rasterizer built in the course homework.

**Report:** [REPORT.md](./REPORT.md)

## Run

```bash
npm install
npm run dev      # open the printed http://localhost:5173 address
npm run build    # type-check and production build
```

## Open source used

[Vite](https://vite.dev) and [TypeScript](https://www.typescriptlang.org).
