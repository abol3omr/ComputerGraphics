#include "MiniFB.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// assignment2
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

extern "C"
{
#include "microui.h"
}
#include "ui_bridge.h"
#include "ui_renderer.h"

#define WIDTH 1600
#define HEIGHT 1200

static uint32_t g_buffer[WIDTH * HEIGHT];
float time_speed = 0.05f;  // controls animation speed
bool frozen = false;       // freeze toggle
bool red_theme = false;    // color theme toggle
float freq_slider = 60.0f; // controls ray width
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
  // part 0: glm example - create a simple transformation matrix
  glm::vec3 position(1.0f, 2.0f, 3.0f);
  glm::mat4 model = glm::mat4(1.0f); // identity matrix
  model = glm::translate(model, position);
  printf("GLM works! Translated position: (%.1f, %.1f, %.1f)\n",
         model[3][0], model[3][1], model[3][2]);

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
