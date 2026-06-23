#include "MiniFB.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <vector>
#include <fstream>
#include <sstream>
#include <string>

// assignment2
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

extern "C"
{
#include "microui.h"
}
#include "ui_bridge.h"
#include "ui_renderer.h"

#define WIDTH 1280
#define HEIGHT 800

// assignment2, part 4: local and world transformation state
glm::vec3 local_translation(0.0f, 0.0f, 0.0f);
glm::vec3 local_rotation(0.0f, 0.0f, 0.0f); // in degrees
glm::vec3 local_scale(1.0f, 1.0f, 1.0f);

glm::vec3 world_translation(0.0f, 0.0f, 0.0f);
glm::vec3 world_rotation(0.0f, 0.0f, 0.0f); // in degrees
glm::vec3 world_scale(1.0f, 1.0f, 1.0f);

static uint32_t g_buffer[WIDTH * HEIGHT];
float time_speed = 0.05f;                 // controls animation speed
bool frozen = false;                      // freeze toggle
bool red_theme = false;                   // color theme toggle
float freq_slider = 60.0f;                // controls ray width
bool show_axes = false;                   // toggle coordinate axes
bool show_bbox = false;                   // toggle bounding box
glm::vec3 cam_position(0.0f, 0.0f, 0.0f); // camera starts behind the scene
glm::vec3 cam_rotation(0.0f, 0.0f, 0.0f); // in degrees
bool show_face_normals = false;           // toggle face normal debug lines
bool show_vertex_normals = false;         // toggle vertex normal debug lines

// hw3 part 2+3: projection settings
float cam_fov = 90.0f;        // field of view in degrees
float cam_near = 0.1f;        // near clipping plane
float cam_far = 10000.0f;     // far clipping plane
bool use_perspective = false; // toggle between ortho and perspective

// line storage
struct Line
{
  int x0, y0, x1, y1;
  uint32_t color;
};
static Line lines[1000];   // permanent lines array
static int line_count = 0; // how many lines are stored

// drawing state
static bool is_drawing = false;
static int draw_start_x = 0;
static int draw_start_y = 0;

// current line color (rgb sliders)
float color_r = 255.0f;
float color_g = 255.0f;
float color_b = 255.0f;

// bresenham's line algorithm
// handles all slopes using switch and reflect approach
void draw_line(int p1, int q1, int p2, int q2, uint32_t color)
{
  int dp = p2 - p1;
  int dq = q2 - q1;

  // determine switch (steep) and reflect (negative direction)
  bool steep = abs(dq) > abs(dp);
  if (steep)
  {
    std::swap(p1, q1);
    std::swap(p2, q2);
    std::swap(dp, dq);
  }

  // reflect: ensure we always go left to right
  if (p1 > p2)
  {
    std::swap(p1, p2);
    std::swap(q1, q2);
  }
  dp = p2 - p1;
  dq = q2 - q1;

  int sy = dq > 0 ? 1 : -1; // direction of y step
  dq = abs(dq);

  // e starts at -1 (as per slides), scaled by 2*dp to avoid floats
  int e = 2 * dq - dp;

  int x = p1, y = q1;
  while (x <= p2)
  {
    // draw pixel, swap back if steep
    int px = steep ? y : x;
    int py = steep ? x : y;
    if (px >= 0 && px < WIDTH && py >= 0 && py < HEIGHT)
      g_buffer[py * WIDTH + px] = color;

    // if e > 0, step y and adjust e
    if (e > 0)
    {
      y += sy;
      e -= 2 * dp;
    }

    // always step x and update e
    x++;
    e += 2 * dq;
  }
}

// 3d vertex
struct Vec3
{
  float x, y, z;
};

// triangle face - stores 3 vertex indices
struct Face
{
  int v0, v1, v2;
};

// obj loader - reads vertices and faces from file
bool load_obj(const char *path, std::vector<Vec3> &verts, std::vector<Face> &faces)
{
  std::ifstream file(path);
  if (!file.is_open())
  {
    printf("failed to open: %s\n", path);
    return false;
  }
  std::string line;
  while (std::getline(file, line))
  {
    std::istringstream ss(line);
    std::string token;
    ss >> token;
    if (token == "v")
    {
      // vertex line
      Vec3 v;
      ss >> v.x >> v.y >> v.z;
      verts.push_back(v);
    }
    else if (token == "f")
    {
      // face line - parse only first number before '/'
      Face f;
      std::string s0, s1, s2;
      ss >> s0 >> s1 >> s2;
      f.v0 = std::stoi(s0) - 1; // obj indices start at 1
      f.v1 = std::stoi(s1) - 1;
      f.v2 = std::stoi(s2) - 1;
      faces.push_back(f);
    }
  }
  printf("loaded %zu vertices and %zu faces\n", verts.size(), faces.size());
  return true;
}

// normalize mesh to fit within window
// returns normalized copy of vertices
std::vector<Vec3> normalize_mesh(const std::vector<Vec3> &verts, int width, int height)
{
  // find bounding box
  float min_x = verts[0].x, max_x = verts[0].x;
  float min_y = verts[0].y, max_y = verts[0].y;
  float min_z = verts[0].z, max_z = verts[0].z;
  for (const auto &v : verts)
  {
    min_x = fminf(min_x, v.x);
    max_x = fmaxf(max_x, v.x);
    min_y = fminf(min_y, v.y);
    max_y = fmaxf(max_y, v.y);
    min_z = fminf(min_z, v.z);
    max_z = fmaxf(max_z, v.z);
  }

  // center of bounding box
  float cx = (min_x + max_x) / 2.0f;
  float cy = (min_y + max_y) / 2.0f;
  float cz = (min_z + max_z) / 2.0f;

  // largest dimension -> scale factor
  float range_x = max_x - min_x;
  float range_y = max_y - min_y;
  float range_z = max_z - min_z;
  float max_range = fmaxf(range_x, fmaxf(range_y, range_z));

  // scale to 80% of window size, centered
  float scale = (fminf(width, height) * 0.8f) / max_range;

  std::vector<Vec3> normalized;
  for (const auto &v : verts)
  {
    Vec3 n;
    n.x = (v.x - cx) * scale + width / 2.0f;
    n.y = (v.y - cy) * scale + height / 2.0f;
    n.z = (v.z - cz) * scale;
    normalized.push_back(n);
  }
  return normalized;
}

// apply local, world, and camera (view) transforms to a vertex
// full pipeline: model -> view -> project -> screen
Vec3 apply_transforms(const Vec3 &v)
{
  glm::vec4 p(v.x, v.y, v.z, 1.0f);

  // --- local transform ---
  glm::mat4 local = glm::mat4(1.0f);
  local = glm::translate(local, local_translation);
  local = glm::rotate(local, glm::radians(local_rotation.x), glm::vec3(1, 0, 0));
  local = glm::rotate(local, glm::radians(local_rotation.y), glm::vec3(0, 1, 0));
  local = glm::rotate(local, glm::radians(local_rotation.z), glm::vec3(0, 0, 1));
  local = glm::scale(local, local_scale);

  // --- world transform ---
  glm::mat4 world = glm::mat4(1.0f);
  world = glm::translate(world, world_translation);
  world = glm::rotate(world, glm::radians(world_rotation.x), glm::vec3(1, 0, 0));
  world = glm::rotate(world, glm::radians(world_rotation.y), glm::vec3(0, 1, 0));
  world = glm::rotate(world, glm::radians(world_rotation.z), glm::vec3(0, 0, 1));
  world = glm::scale(world, world_scale);

  // --- view (camera) transform ---
  glm::mat4 view = glm::mat4(1.0f);
  view = glm::rotate(view, glm::radians(-cam_rotation.x), glm::vec3(1, 0, 0));
  view = glm::rotate(view, glm::radians(-cam_rotation.y), glm::vec3(0, 1, 0));
  view = glm::rotate(view, glm::radians(-cam_rotation.z), glm::vec3(0, 0, 1));
  view = glm::translate(view, -cam_position);

  // apply model and view
  glm::vec4 view_pos = view * world * local * p;

  // --- projection ---
  float aspect = (float)WIDTH / (float)HEIGHT;
  glm::vec4 proj_pos;

  if (use_perspective)
  {
    glm::mat4 proj = glm::perspective(glm::radians(cam_fov), aspect, cam_near, cam_far);
    proj_pos = proj * view_pos;
    // perspective divide
    if (proj_pos.w != 0.0f)
    {
      proj_pos.x /= proj_pos.w;
      proj_pos.y /= proj_pos.w;
    }
    // convert NDC (-1 to 1) to screen pixels
    float sx = (proj_pos.x + 1.0f) * 0.5f * WIDTH;
    float sy = (1.0f - proj_pos.y) * 0.5f * HEIGHT;
    return Vec3{sx, sy, proj_pos.z};
  }
  else
  {
    // orthographic: scale and center, no perspective divide
    float scale = 300.0f;
    float sx = view_pos.x * scale + WIDTH / 2.0f;
    float sy = -view_pos.y * scale + HEIGHT / 2.0f;
    return Vec3{sx, sy, view_pos.z};
  }
}

// compute face normal using cross product of triangle edges
Vec3 compute_face_normal(const Vec3 &v0, const Vec3 &v1, const Vec3 &v2)
{
  glm::vec3 a(v1.x - v0.x, v1.y - v0.y, v1.z - v0.z);
  glm::vec3 b(v2.x - v0.x, v2.y - v0.y, v2.z - v0.z);
  glm::vec3 n = glm::normalize(glm::cross(a, b));
  return Vec3{n.x, n.y, n.z};
}

int main()
{
  struct mfb_window *window =
      mfb_open_ex("MiniGUI Platform", WIDTH, HEIGHT, MFB_WF_RESIZABLE);
  if (!window)
    return 1;

  mu_Context *ctx = (mu_Context *)malloc(sizeof(mu_Context));
  mu_init(ctx);

  // Set font callbacks for microui
  ctx->text_width = [](mu_Font font, const char *str, int len)
  {
    return (len < 0 ? (int)strlen(str) : len) * 8;
  };
  ctx->text_height = [](mu_Font font)
  { return 8; };

  UIRenderer renderer(WIDTH, HEIGHT);

  // added for part1
  float time = 0.0f;
  // added for part3

  // Set up char input callback for textbox input
  mfb_set_char_input_callback(
      [](struct mfb_window *w, unsigned int c)
      {
        extern void ui_bridge_char_input(struct mfb_window *, unsigned int);
        extern float time_speed;
        extern bool frozen;
        extern bool red_theme;
        if (c == 'a')
          time_speed = fminf(time_speed + 0.02f, 0.3f); // increase speed
        else if (c == 'd')
          time_speed = fmaxf(time_speed - 0.02f, 0.01f); // decrease speed
        else if (c == 'r')
          red_theme = !red_theme; // toggle color theme
        else if (c == ' ')
          frozen = !frozen; // freeze/unfreeze
        else
          ui_bridge_char_input(w, c); // pass other keys to UI
      },
      window);
  // part 6: arrow keys control world translation
  mfb_set_keyboard_callback(
      [](struct mfb_window *w, mfb_key key, mfb_key_mod mod, bool isPressed)
      {
        if (!isPressed)
          return; // only react on key press, not release
        extern glm::vec3 world_translation;
        const float step = 0.1f;
        if (key == KB_KEY_RIGHT)
          world_translation.x += step;
        else if (key == KB_KEY_LEFT)
          world_translation.x -= step;
        else if (key == KB_KEY_UP)
          world_translation.y -= step; // screen y is flipped
        else if (key == KB_KEY_DOWN)
          world_translation.y += step;
      },
      window);
  // part 0: glm example - create a simple transformation matrix
  glm::vec3 position(1.0f, 2.0f, 3.0f);
  glm::mat4 model = glm::mat4(1.0f); // identity matrix
  model = glm::translate(model, position);
  printf("GLM works! Translated position: (%.1f, %.1f, %.1f)\n",
         model[3][0], model[3][1], model[3][2]);

  // part 1: load obj file
  std::vector<Vec3> mesh_verts;
  std::vector<Face> mesh_faces;
  load_obj("cube.obj", mesh_verts, mesh_faces);

  // part 2: normalize mesh to fit window
  std::vector<Vec3> norm_verts = normalize_mesh(mesh_verts, WIDTH, HEIGHT);

  while (mfb_update_events(window) != MFB_STATE_EXIT)
  {
    // 1. Input
    ui_bridge_input(ctx, window);

    // 2. Scene Rendering (Background)
    for (int i = 0; i < WIDTH * HEIGHT; i++)
    {
      // Simple gradient background
      int x = i % WIDTH;
      int y = i / WIDTH;

      // diagonal rays using x and y combined
      float diag = (x + y * 0.6f) / freq_slider + time;
      // two overlapping ray frequencies for complexity
      float ray1 = sinf(diag) * 0.5f + 0.5f;
      float ray2 = sinf(diag * 2.3f + 1.0f) * 0.3f + 0.3f;
      float t = fminf(1.0f, ray1 * 0.7f + ray2);

      // initial colors: navy, sky blue, white
      uint8_t r, g, b;
      if (!red_theme)
      {
        // initial colors: navy, sky blue, white
        if (t < 0.5f)
        {
          float s = t / 0.5f;
          r = (uint8_t)(28 + (108 - 28) * s);
          g = (uint8_t)(44 + (171 - 44) * s);
          b = (uint8_t)(91 + (221 - 91) * s);
        }
        else
        {
          float s = (t - 0.5f) / 0.5f;
          r = (uint8_t)(108 + (255 - 108) * s);
          g = (uint8_t)(171 + (255 - 171) * s);
          b = (uint8_t)(221 + (255 - 221) * s);
        }
      }
      else
      {
        // red theme: black, red, crimson
        if (t < 0.5f)
        {
          float s = t / 0.5f;
          r = (uint8_t)(0 + (200 - 0) * s);
          g = (uint8_t)(0 + (30 - 0) * s);
          b = (uint8_t)(0 + (30 - 0) * s);
        }
        else
        {
          float s = (t - 0.5f) / 0.5f;
          r = (uint8_t)(200 + (255 - 200) * s);
          g = (uint8_t)(30 + (80 - 30) * s);
          b = (uint8_t)(30 + (80 - 30) * s);
        }
      }
      g_buffer[i] = MFB_RGB(r, g, b);
    }

    if (!frozen)
      time += time_speed;

    // part 6

    // draw all permanent lines
    for (int i = 0; i < line_count; i++)
    {
      draw_line(lines[i].x0, lines[i].y0, lines[i].x1, lines[i].y1, lines[i].color);
    }

    // get current mouse position and button state from microui
    int mx = ctx->mouse_pos.x;
    int my = ctx->mouse_pos.y;
    bool mouse_down = ctx->mouse_down & MU_MOUSE_LEFT;

    // handle drawing state
    bool over_ui = (ctx->hover_root != nullptr);

    if (mouse_down && !is_drawing && !over_ui)
    {
      // start drawing only if not over UI
      is_drawing = true;
      draw_start_x = mx;
      draw_start_y = my;
    }
    else if (!mouse_down && is_drawing)
    {
      // always save line on release, wherever mouse is
      if (line_count < 1000)
      {
        lines[line_count++] = {
            draw_start_x, draw_start_y, mx, my,
            MFB_RGB((uint8_t)color_r, (uint8_t)color_g, (uint8_t)color_b)};
      }
      is_drawing = false;
    }

    if (is_drawing)
    {
      draw_line(draw_start_x, draw_start_y, mx, my,
                MFB_RGB((uint8_t)color_r, (uint8_t)color_g, (uint8_t)color_b));
    }

    // part 3: draw wireframe using orthographic projection (drop z)
    // part 3 + 5: draw wireframe with transforms applied
    for (const auto &face : mesh_faces)
    {
      Vec3 v0 = apply_transforms(mesh_verts[face.v0]);
      Vec3 v1 = apply_transforms(mesh_verts[face.v1]);
      Vec3 v2 = apply_transforms(mesh_verts[face.v2]);

      draw_line((int)v0.x, (int)v0.y, (int)v1.x, (int)v1.y, MFB_RGB(255, 255, 255));
      draw_line((int)v1.x, (int)v1.y, (int)v2.x, (int)v2.y, MFB_RGB(255, 255, 255));
      draw_line((int)v2.x, (int)v2.y, (int)v0.x, (int)v0.y, MFB_RGB(255, 255, 255));
    }

    // hw3 part 1: draw coordinate axes and bounding box
    if (show_axes)
    {
      // world axes at origin (0,0,0) -> screen center
      int ox = WIDTH / 2, oy = HEIGHT / 2;
      int axis_len = 100;
      // x axis - red
      draw_line(ox, oy, ox + axis_len, oy, MFB_RGB(255, 0, 0));
      // y axis - green (screen y is flipped)
      draw_line(ox, oy, ox, oy - axis_len, MFB_RGB(0, 255, 0));
      // z axis - blue (projected diagonally)
      draw_line(ox, oy, ox + axis_len / 2, oy + axis_len / 2, MFB_RGB(0, 0, 255));

      // local axes in model space (cube is -1 to 1)
      float axis_size = 0.5f;
      Vec3 center = apply_transforms({0.0f, 0.0f, 0.0f});
      Vec3 x_tip = apply_transforms({axis_size, 0.0f, 0.0f});
      Vec3 y_tip = apply_transforms({0.0f, axis_size, 0.0f});
      Vec3 z_tip = apply_transforms({0.0f, 0.0f, axis_size});
      draw_line((int)center.x, (int)center.y, (int)x_tip.x, (int)x_tip.y, MFB_RGB(255, 100, 100));
      draw_line((int)center.x, (int)center.y, (int)y_tip.x, (int)y_tip.y, MFB_RGB(100, 255, 100));
      draw_line((int)center.x, (int)center.y, (int)z_tip.x, (int)z_tip.y, MFB_RGB(100, 100, 255));
    }

    // hw3 part 4: draw face and vertex normals
    if (show_face_normals || show_vertex_normals)
    {
      float normal_len = 0.3f; // length of normal lines in model space

      // compute vertex normals by averaging face normals
      std::vector<glm::vec3> vert_normals(mesh_verts.size(), glm::vec3(0.0f));

      for (const auto &face : mesh_faces)
      {
        Vec3 v0 = mesh_verts[face.v0];
        Vec3 v1 = mesh_verts[face.v1];
        Vec3 v2 = mesh_verts[face.v2];

        Vec3 fn = compute_face_normal(v0, v1, v2);

        if (show_face_normals)
        {
          // face center
          Vec3 fc = {(v0.x + v1.x + v2.x) / 3.0f, (v0.y + v1.y + v2.y) / 3.0f, (v0.z + v1.z + v2.z) / 3.0f};
          Vec3 fc_tip = {fc.x + fn.x * normal_len, fc.y + fn.y * normal_len, fc.z + fn.z * normal_len};
          Vec3 pfc = apply_transforms(fc);
          Vec3 ptip = apply_transforms(fc_tip);
          draw_line((int)pfc.x, (int)pfc.y, (int)ptip.x, (int)ptip.y, MFB_RGB(255, 128, 0));
        }

        // accumulate for vertex normals
        vert_normals[face.v0] += glm::vec3(fn.x, fn.y, fn.z);
        vert_normals[face.v1] += glm::vec3(fn.x, fn.y, fn.z);
        vert_normals[face.v2] += glm::vec3(fn.x, fn.y, fn.z);
      }

      if (show_vertex_normals)
      {
        for (int i = 0; i < (int)mesh_verts.size(); i++)
        {
          glm::vec3 vn = glm::normalize(vert_normals[i]);
          Vec3 v = mesh_verts[i];
          Vec3 v_tip = {v.x + vn.x * normal_len, v.y + vn.y * normal_len, v.z + vn.z * normal_len};
          Vec3 pv = apply_transforms(v);
          Vec3 ptip = apply_transforms(v_tip);
          draw_line((int)pv.x, (int)pv.y, (int)ptip.x, (int)ptip.y, MFB_RGB(0, 255, 255));
        }
      }
    }

    if (show_bbox)
    {
      // compute bounding box in original model space
      float min_x = mesh_verts[0].x, max_x = mesh_verts[0].x;
      float min_y = mesh_verts[0].y, max_y = mesh_verts[0].y;
      float min_z = mesh_verts[0].z, max_z = mesh_verts[0].z;
      for (const auto &v : mesh_verts)
      {
        min_x = fminf(min_x, v.x);
        max_x = fmaxf(max_x, v.x);
        min_y = fminf(min_y, v.y);
        max_y = fmaxf(max_y, v.y);
        min_z = fminf(min_z, v.z);
        max_z = fmaxf(max_z, v.z);
      }

      // 8 corners in model space, then project each through full pipeline
      Vec3 raw_corners[8] = {
          {min_x, min_y, min_z}, {max_x, min_y, min_z}, {max_x, max_y, min_z}, {min_x, max_y, min_z}, {min_x, min_y, max_z}, {max_x, min_y, max_z}, {max_x, max_y, max_z}, {min_x, max_y, max_z}};

      Vec3 corners[8];
      for (int i = 0; i < 8; i++)
        corners[i] = apply_transforms(raw_corners[i]);

      uint32_t bbox_color = MFB_RGB(255, 255, 0);
      // bottom face
      draw_line((int)corners[0].x, (int)corners[0].y, (int)corners[1].x, (int)corners[1].y, bbox_color);
      draw_line((int)corners[1].x, (int)corners[1].y, (int)corners[2].x, (int)corners[2].y, bbox_color);
      draw_line((int)corners[2].x, (int)corners[2].y, (int)corners[3].x, (int)corners[3].y, bbox_color);
      draw_line((int)corners[3].x, (int)corners[3].y, (int)corners[0].x, (int)corners[0].y, bbox_color);
      // top face
      draw_line((int)corners[4].x, (int)corners[4].y, (int)corners[5].x, (int)corners[5].y, bbox_color);
      draw_line((int)corners[5].x, (int)corners[5].y, (int)corners[6].x, (int)corners[6].y, bbox_color);
      draw_line((int)corners[6].x, (int)corners[6].y, (int)corners[7].x, (int)corners[7].y, bbox_color);
      draw_line((int)corners[7].x, (int)corners[7].y, (int)corners[4].x, (int)corners[4].y, bbox_color);
      // vertical edges
      draw_line((int)corners[0].x, (int)corners[0].y, (int)corners[4].x, (int)corners[4].y, bbox_color);
      draw_line((int)corners[1].x, (int)corners[1].y, (int)corners[5].x, (int)corners[5].y, bbox_color);
      draw_line((int)corners[2].x, (int)corners[2].y, (int)corners[6].x, (int)corners[6].y, bbox_color);
      draw_line((int)corners[3].x, (int)corners[3].y, (int)corners[7].x, (int)corners[7].y, bbox_color);
    }

    // 3. UI Logic
    static float slider_val = 50.0f;
    static float number_val = 3.14f;
    static int checkbox_a = 0;
    static int checkbox_b = 1;
    static char textbox_buf[128] = "edit me";
    static bool quit_requested = false;

    mu_begin(ctx);

    // --- Widgets window ---
    if (mu_begin_window(ctx, "Widgets", mu_rect(20, 20, 360, 540)))
    {
      int w1[] = {-1};

      // label / text
      mu_layout_row(ctx, 1, w1, 0);
      mu_label(ctx, "mu_label: plain static text");
      mu_text(ctx, "mu_text: word-wrapped longer text that will reflow inside "
                   "the window width automatically.");

      // button
      // toggle welcome message on screen
      static bool show_welcome = false;
      mu_layout_row(ctx, 1, w1, 0);
      if (mu_button(ctx, "Welcome!"))
      {
        printf("Welcome to my assignment!!\n");
        show_welcome = !show_welcome;
      }
      if (show_welcome)
      {
        mu_label(ctx, "Welcome to my assignment!");
      }

      // speed slider
      mu_layout_row(ctx, 1, w1, 0);
      mu_label(ctx, "Ray Speed:");
      mu_slider(ctx, &time_speed, 0.01f, 0.3f);

      // frequency slider
      mu_layout_row(ctx, 1, w1, 0);
      mu_label(ctx, "Ray Width:");
      mu_slider(ctx, &freq_slider, 20.0f, 120.0f);

      // checkbox
      mu_layout_row(ctx, 1, w1, 0);
      mu_checkbox(ctx, "mu_checkbox A (off)", &checkbox_a);
      mu_checkbox(ctx, "mu_checkbox B (on)", &checkbox_b);

      // textbox
      mu_layout_row(ctx, 1, w1, 0);
      mu_label(ctx, "mu_textbox:");
      mu_textbox(ctx, textbox_buf, sizeof(textbox_buf));

      // slider
      mu_layout_row(ctx, 1, w1, 0);
      mu_label(ctx, "mu_slider (0-100):");
      mu_slider(ctx, &slider_val, 0, 100);

      // number
      mu_layout_row(ctx, 1, w1, 0);
      mu_label(ctx, "mu_number (step 0.1):");
      mu_number(ctx, &number_val, 0.1f);

      // part 1: show mesh info
      mu_layout_row(ctx, 1, w1, 0);
      char mesh_info[64];
      snprintf(mesh_info, sizeof(mesh_info), "Vertices: %d", (int)mesh_verts.size());
      mu_label(ctx, mesh_info);

      snprintf(mesh_info, sizeof(mesh_info), "Faces: %d", (int)mesh_faces.size());
      mu_label(ctx, mesh_info);

      // hw3 part 1: debug toggles
      mu_layout_row(ctx, 1, w1, 0);
      mu_checkbox(ctx, "Show Axes", (int *)&show_axes);
      mu_checkbox(ctx, "Show Bounding Box", (int *)&show_bbox);
      mu_checkbox(ctx, "Show Face Normals", (int *)&show_face_normals);
      mu_checkbox(ctx, "Show Vertex Normals", (int *)&show_vertex_normals);

      // header (collapsible section)
      if (mu_header(ctx, "mu_header: collapsible section"))
      {
        mu_layout_row(ctx, 1, w1, 0);
        mu_label(ctx, "Content inside the header.");
      }

      // treenode
      if (mu_begin_treenode(ctx, "mu_treenode: root"))
      {
        mu_layout_row(ctx, 1, w1, 0);
        mu_label(ctx, "child item A");
        if (mu_begin_treenode(ctx, "nested node"))
        {
          mu_layout_row(ctx, 1, w1, 0);
          mu_label(ctx, "deeply nested item");
          mu_end_treenode(ctx);
        }
        mu_end_treenode(ctx);
      }

      // quit button
      mu_layout_row(ctx, 1, w1, 0);
      if (mu_button(ctx, "Quit"))
      {
        quit_requested = true;
      }

      mu_end_window(ctx);
    }

    // --- Panel window ---
    if (mu_begin_window(ctx, "Panel Demo", mu_rect(395, 20, 380, 200)))
    {
      int w2[] = {-1};
      mu_layout_row(ctx, 1, w2, 120);
      mu_begin_panel(ctx, "scrollable panel");
      int wp[] = {-1};
      for (int i = 1; i <= 12; i++)
      {
        mu_layout_row(ctx, 1, wp, 0);
        char line[32];
        snprintf(line, sizeof(line), "Panel row %d", i);
        mu_label(ctx, line);
      }
      mu_end_panel(ctx);
      mu_end_window(ctx);
    }

    // --- Popup demo window ---
    if (mu_begin_window(ctx, "Popup Demo", mu_rect(395, 235, 380, 80)))
    {
      int w3[] = {-1};
      mu_layout_row(ctx, 1, w3, 0);
      if (mu_button(ctx, "Open popup"))
      {
        mu_Container *popup = mu_get_container(ctx, "my popup");
        popup->rect = mu_rect(ctx->mouse_pos.x, ctx->mouse_pos.y, 260, 84);
        popup->open = 1;
        ctx->hover_root = ctx->next_hover_root = popup;
        mu_bring_to_front(ctx, popup);
      }
      int popup_opt = MU_OPT_POPUP | MU_OPT_NORESIZE | MU_OPT_NOSCROLL |
                      MU_OPT_NOTITLE | MU_OPT_CLOSED;
      if (mu_begin_window_ex(ctx, "my popup", mu_rect(0, 0, 260, 84),
                             popup_opt))
      {
        int wp[] = {-1};
        mu_layout_row(ctx, 1, wp, 0);
        mu_label(ctx, "mu_popup: click outside to close");
        if (mu_button(ctx, "Close"))
        {
          mu_get_current_container(ctx)->open = 0;
        }
        mu_end_window(ctx);
      }
      mu_end_window(ctx);
    }
    // --- Drawing Tools window ---
    if (mu_begin_window(ctx, "Drawing Tools", mu_rect(395, 330, 380, 200)))
    {
      int wd[] = {-1};

      // r slider
      mu_layout_row(ctx, 1, wd, 0);
      mu_label(ctx, "Red:");
      mu_slider(ctx, &color_r, 0.0f, 255.0f);

      // g slider
      mu_layout_row(ctx, 1, wd, 0);
      mu_label(ctx, "Green:");
      mu_slider(ctx, &color_g, 0.0f, 255.0f);

      // b slider
      mu_layout_row(ctx, 1, wd, 0);
      mu_label(ctx, "Blue:");
      mu_slider(ctx, &color_b, 0.0f, 255.0f);

      // clear button
      mu_layout_row(ctx, 1, wd, 0);
      if (mu_button(ctx, "Clear Screen"))
      {
        line_count = 0;
      }

      mu_end_window(ctx);
    }
    // --- Transform Tools window ---
    if (mu_begin_window(ctx, "Transform Tools", mu_rect(20, 580, 700, 400)))
    {
      int wt[] = {-1};
      int w3[] = {220, 220, -1};

      mu_label(ctx, "-- Local Transform --");

      mu_label(ctx, "Local Translate X/Y/Z:");
      mu_layout_row(ctx, 3, w3, 0);
      mu_slider(ctx, &local_translation.x, -3.0f, 3.0f);
      mu_slider(ctx, &local_translation.y, -3.0f, 3.0f);
      mu_slider(ctx, &local_translation.z, -3.0f, 3.0f);

      mu_layout_row(ctx, 1, wt, 0);
      mu_label(ctx, "Local Rotate X/Y/Z:");
      mu_layout_row(ctx, 3, w3, 0);
      mu_slider(ctx, &local_rotation.x, -180.0f, 180.0f);
      mu_slider(ctx, &local_rotation.y, -180.0f, 180.0f);
      mu_slider(ctx, &local_rotation.z, -180.0f, 180.0f);

      mu_layout_row(ctx, 1, wt, 0);
      mu_label(ctx, "Local Scale X/Y/Z:");
      mu_layout_row(ctx, 3, w3, 0);
      mu_slider(ctx, &local_scale.x, 0.1f, 3.0f);
      mu_slider(ctx, &local_scale.y, 0.1f, 3.0f);
      mu_slider(ctx, &local_scale.z, 0.1f, 3.0f);

      mu_layout_row(ctx, 1, wt, 0);
      mu_label(ctx, "-- World Transform --");

      mu_label(ctx, "World Translate X/Y/Z:");
      mu_layout_row(ctx, 3, w3, 0);
      mu_slider(ctx, &world_translation.x, -3.0f, 3.0f);
      mu_slider(ctx, &world_translation.y, -3.0f, 3.0f);
      mu_slider(ctx, &world_translation.z, -3.0f, 3.0f);

      mu_layout_row(ctx, 1, wt, 0);
      mu_label(ctx, "World Rotate X/Y/Z:");
      mu_layout_row(ctx, 3, w3, 0);
      mu_slider(ctx, &world_rotation.x, -180.0f, 180.0f);
      mu_slider(ctx, &world_rotation.y, -180.0f, 180.0f);
      mu_slider(ctx, &world_rotation.z, -180.0f, 180.0f);

      mu_layout_row(ctx, 1, wt, 0);
      mu_label(ctx, "World Scale X/Y/Z:");
      mu_layout_row(ctx, 3, w3, 0);
      mu_slider(ctx, &world_scale.x, 0.1f, 3.0f);
      mu_slider(ctx, &world_scale.y, 0.1f, 3.0f);
      mu_slider(ctx, &world_scale.z, 0.1f, 3.0f);

      mu_end_window(ctx);
    }

    // --- Camera window ---
    if (mu_begin_window(ctx, "Camera", mu_rect(730, 20, 500, 200)))
    {
      int wc[] = {-1};
      int w3[] = {150, 150, -1};

      mu_label(ctx, "Camera Position X/Y/Z:");
      mu_layout_row(ctx, 3, w3, 0);
      mu_slider(ctx, &cam_position.x, -5.0f, 5.0f);
      mu_slider(ctx, &cam_position.y, -5.0f, 5.0f);
      mu_slider(ctx, &cam_position.z, -5.0f, 5.0f);

      mu_layout_row(ctx, 1, wc, 0);
      mu_label(ctx, "Camera Rotation X/Y/Z:");
      mu_layout_row(ctx, 3, w3, 0);
      mu_slider(ctx, &cam_rotation.x, -180.0f, 180.0f);
      mu_slider(ctx, &cam_rotation.y, -180.0f, 180.0f);
      mu_slider(ctx, &cam_rotation.z, -180.0f, 180.0f);

      // perspective toggle and fov
      mu_layout_row(ctx, 1, wc, 0);
      mu_checkbox(ctx, "Perspective Mode", (int *)&use_perspective);

      mu_layout_row(ctx, 1, wc, 0);
      mu_label(ctx, "Field of View:");
      mu_slider(ctx, &cam_fov, 10.0f, 170.0f);

      mu_end_window(ctx);
    }

    mu_end(ctx);

    if (quit_requested)
    {
      mfb_close(window);
      break;
    }

    // 4. UI Rendering
    renderer.render(ctx, g_buffer);

    // 5. Display
    mfb_update_state state = mfb_update_ex(window, g_buffer, WIDTH, HEIGHT);
    if (state < 0)
      break;

    // Cap FPS (optional, minifb has built-in sync)
    mfb_wait_sync(window);
  }

  mfb_close(window);
  free(ctx);
  return 0;
}
