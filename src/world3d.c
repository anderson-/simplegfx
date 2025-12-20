#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <world3d.h>

#define DEG2RAD (M_PI / 180.0f)

static int clamp_color(int v) {
  if (v < 0) return 0;
  if (v > 255) return 255;
  return v;
}

static w3_vec3 v_add(w3_vec3 a, w3_vec3 b) {
  w3_vec3 r = {a.x + b.x, a.y + b.y, a.z + b.z};
  return r;
}

static w3_vec3 v_sub(w3_vec3 a, w3_vec3 b) {
  w3_vec3 r = {a.x - b.x, a.y - b.y, a.z - b.z};
  return r;
}

static w3_vec3 v_scale(w3_vec3 v, float s) {
  w3_vec3 r = {v.x * s, v.y * s, v.z * s};
  return r;
}

static float v_len(w3_vec3 v) {
  return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

static w3_vec3 v_norm(w3_vec3 v) {
  float l = v_len(v);
  if (l <= 0.0001f) return v;
  return v_scale(v, 1.0f / l);
}

static float frand(float min, float max) {
  float t = (float)rand() / 32767.0f;
  return min + t * (max - min);
}

static w3_vec3 camera_forward(const w3_state *s) {
  float cy = cosf(s->camera.yaw);
  float sy = sinf(s->camera.yaw);
  float cp = cosf(s->camera.pitch);
  float sp = sinf(s->camera.pitch);
  w3_vec3 f = {cy * cp, sp, sy * cp};
  return v_norm(f);
}

static w3_vec3 camera_right(const w3_state *s) {
  w3_vec3 f = camera_forward(s);
  w3_vec3 r = {f.z, 0.0f, -f.x};
  return v_norm(r);
}

static int push_object(w3_state *s, w3_object obj) {
  if (s->object_count >= W3_MAX_OBJECTS) {
    return -1;
  }
  s->objects[s->object_count] = obj;
  s->selected = s->object_count;
  s->object_count++;
  return s->selected;
}

void w3_reset_objects(w3_state *s) {
  s->object_count = 0;
  s->selected = -1;
  s->label_counter = 0;
}

static w3_vec3 place_in_front(const w3_state *s, float min_dist, float max_dist) {
  w3_vec3 dir = camera_forward(s);
  float dist = frand(min_dist, max_dist);
  return v_add(s->camera.pos, v_scale(dir, dist));
}

int w3_add_line(w3_state *s, w3_vec3 a, w3_vec3 b, int r, int g, int bcol) {
  w3_object obj;
  memset(&obj, 0, sizeof(obj));
  obj.kind = W3_OBJ_LINE;
  obj.r = r;
  obj.g = g;
  obj.b = bcol;
  obj.data.line.a = a;
  obj.data.line.b = b;
  return push_object(s, obj);
}

int w3_add_sphere(w3_state *s, w3_vec3 pos, float radius, int r, int g, int bcol) {
  w3_object obj;
  memset(&obj, 0, sizeof(obj));
  obj.kind = W3_OBJ_SPHERE;
  obj.r = r;
  obj.g = g;
  obj.b = bcol;
  obj.data.sphere.pos = pos;
  obj.data.sphere.radius = radius;
  return push_object(s, obj);
}

int w3_add_text(w3_state *s, w3_vec3 pos, const char *label, int r, int g, int bcol) {
  w3_object obj;
  memset(&obj, 0, sizeof(obj));
  obj.kind = W3_OBJ_TEXT;
  obj.r = r;
  obj.g = g;
  obj.b = bcol;
  obj.data.text.pos = pos;
  if (label) {
    strncpy(obj.data.text.label, label, sizeof(obj.data.text.label) - 1);
  } else {
    snprintf(obj.data.text.label, sizeof(obj.data.text.label), "note %d", ++s->label_counter);
  }
  return push_object(s, obj);
}

int w3_add_line_in_front(w3_state *s, float min_dist, float max_dist) {
  w3_vec3 start = place_in_front(s, min_dist, max_dist);
  w3_vec3 dir = camera_forward(s);
  w3_vec3 jitter = {frand(-1.5f, 1.5f), frand(-1.0f, 1.0f), frand(-1.5f, 1.5f)};
  w3_vec3 end = v_add(start, v_add(v_scale(dir, frand(1.0f, 3.0f)), jitter));
  int r = (int)frand(100, 255);
  int g = (int)frand(80, 200);
  int b = (int)frand(100, 255);
  return w3_add_line(s, start, end, r, g, b);
}

int w3_add_sphere_in_front(w3_state *s, float min_dist, float max_dist) {
  w3_vec3 pos = v_add(place_in_front(s, min_dist, max_dist),
                      (w3_vec3){frand(-1.5f, 1.5f), frand(-0.5f, 1.5f), frand(-1.5f, 1.5f)});
  float radius = frand(0.5f, 1.6f);
  int r = (int)frand(120, 255);
  int g = (int)frand(120, 255);
  int b = (int)frand(120, 255);
  return w3_add_sphere(s, pos, radius, r, g, b);
}

int w3_add_text_in_front(w3_state *s, const char *label) {
  w3_vec3 pos = v_add(place_in_front(s, 3.0f, 6.0f),
                      (w3_vec3){frand(-1.0f, 1.0f), frand(-1.0f, 1.5f), frand(-1.0f, 1.0f)});
  char tmp[48];
  if (label) {
    strncpy(tmp, label, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = 0;
  } else {
    snprintf(tmp, sizeof(tmp), "note %d", ++s->label_counter);
  }
  return w3_add_text(s, pos, tmp, 255, 220, 80);
}

void w3_remove_selected(w3_state *s) {
  if (s->object_count == 0) {
    s->selected = -1;
    return;
  }
  int idx = s->selected >= 0 ? s->selected : s->object_count - 1;
  if (idx < 0 || idx >= s->object_count) {
    idx = s->object_count - 1;
  }
  for (int i = idx; i < s->object_count - 1; i++) {
    s->objects[i] = s->objects[i + 1];
  }
  s->object_count--;
  if (s->object_count == 0) {
    s->selected = -1;
  } else if (s->selected >= s->object_count) {
    s->selected = s->object_count - 1;
  }
}

void w3_cycle_selection(w3_state *s, int dir) {
  if (s->object_count == 0) {
    s->selected = -1;
    return;
  }
  if (s->selected < 0) {
    s->selected = 0;
  } else {
    s->selected = (s->selected + dir + s->object_count) % s->object_count;
  }
}

void w3_set_help(w3_state *s, int enabled) {
  s->show_help = enabled ? 1 : 0;
}

void w3_set_crosshair(w3_state *s, int enabled) {
  s->show_crosshair = enabled ? 1 : 0;
}

static int project_point(const w3_state *s, w3_vec3 p, int *sx, int *screen_y, float *depth) {
  w3_vec3 rel = v_sub(p, s->camera.pos);

  float cy = cosf(s->camera.yaw);
  float syaw = sinf(s->camera.yaw);
  float cp = cosf(s->camera.pitch);
  float sp = sinf(s->camera.pitch);

  float dx = rel.x * cy - rel.z * syaw;
  float dz = rel.x * syaw + rel.z * cy;

  float dy = rel.y;
  float dy2 = dy * cp - dz * sp;
  float dz2 = dy * sp + dz * cp;

  if (dz2 < W3_NEAR_PLANE || dz2 > W3_FAR_PLANE) {
    return 0;
  }

  float scale = (0.5f * WINDOW_HEIGHT) / tanf(W3_FOV_DEG * DEG2RAD * 0.5f);
  *sx = (int)(WINDOW_WIDTH / 2 + (dx * scale) / dz2);
  *screen_y = (int)(WINDOW_HEIGHT / 2 - (dy2 * scale) / dz2);
  if (depth) *depth = dz2;
  return 1;
}

static void draw_line2d(int x0, int y0, int x1, int y1) {
  int dx = abs(x1 - x0);
  int sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0);
  int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  while (1) {
    gfx_point(x0, y0);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

static void draw_circle(int cx, int cy, int radius) {
  int x = radius;
  int y = 0;
  int err = 0;

  while (x >= y) {
    gfx_point(cx + x, cy + y);
    gfx_point(cx + y, cy + x);
    gfx_point(cx - y, cy + x);
    gfx_point(cx - x, cy + y);
    gfx_point(cx - x, cy - y);
    gfx_point(cx - y, cy - x);
    gfx_point(cx + y, cy - x);
    gfx_point(cx + x, cy - y);

    y += 1;
    if (err <= 0) {
      err += 2 * y + 1;
    } else {
      x -= 1;
      err -= 2 * x + 1;
    }
  }
}

static void draw_crosshair(void) {
  int cx = WINDOW_WIDTH / 2;
  int cy = WINDOW_HEIGHT / 2;
  gfx_set_color(255, 255, 255);
  draw_line2d(cx - 8, cy, cx + 8, cy);
  draw_line2d(cx, cy - 8, cx, cy + 8);
}

static void draw_object(const w3_state *s, w3_object *obj, int highlighted) {
  gfx_set_color(clamp_color(obj->r + (highlighted ? 30 : 0)),
                clamp_color(obj->g + (highlighted ? 30 : 0)),
                clamp_color(obj->b + (highlighted ? 30 : 0)));

  if (obj->kind == W3_OBJ_LINE) {
    int x0, y0, x1, y1;
    float d0 = 0.0f, d1 = 0.0f;
    if (project_point(s, obj->data.line.a, &x0, &y0, &d0) &&
        project_point(s, obj->data.line.b, &x1, &y1, &d1)) {
      draw_line2d(x0, y0, x1, y1);
    }
  } else if (obj->kind == W3_OBJ_SPHERE) {
    int sx, sy;
    float depth = 0.0f;
    if (project_point(s, obj->data.sphere.pos, &sx, &sy, &depth)) {
      float scale = (0.5f * WINDOW_HEIGHT) / tanf(W3_FOV_DEG * DEG2RAD * 0.5f);
      int radius = (int)((obj->data.sphere.radius * scale) / depth);
      if (radius < 1) radius = 1;
      draw_circle(sx, sy, radius);
    }
  } else if (obj->kind == W3_OBJ_TEXT) {
    int sx, sy;
    if (project_point(s, obj->data.text.pos, &sx, &sy, NULL)) {
      gfx_text(obj->data.text.label, sx, sy, 2);
    }
  }
}

static void update_camera(w3_state *s, float dt) {
  float move_speed = s->input.slow ? s->camera.slow_speed : s->camera.move_speed;
  float look_speed = s->camera.look_speed;
  w3_vec3 move = (w3_vec3){0, 0, 0};
  w3_vec3 forward = camera_forward(s);
  w3_vec3 right = camera_right(s);

  if (s->input.forward) move = v_add(move, forward);
  if (s->input.backward) move = v_sub(move, forward);
  if (s->input.left) move = v_sub(move, right);
  if (s->input.right) move = v_add(move, right);
  if (s->input.up) move.y += 1.0f;
  if (s->input.down) move.y -= 1.0f;

  float len = v_len(move);
  if (len > 0.001f) {
    move = v_scale(move, 1.0f / len);
    s->camera.pos = v_add(s->camera.pos, v_scale(move, move_speed * dt));
  }

  s->camera.yaw += (s->input.yaw_right - s->input.yaw_left) * look_speed * dt;
  s->camera.pitch += (s->input.pitch_up - s->input.pitch_down) * look_speed * dt;
  float limit = DEG2RAD * 89.0f;
  if (s->camera.pitch > limit) s->camera.pitch = limit;
  if (s->camera.pitch < -limit) s->camera.pitch = -limit;
}

static void draw_overlay(const w3_state *s, float fps) {
  char line[128];
  int y = 10;
  gfx_set_color(255, 255, 255);
  snprintf(line, sizeof(line), "fps %.1f | objs %d/%d", fps, s->object_count, W3_MAX_OBJECTS);
  y = gfx_text(line, 10, y, 2);
  snprintf(line, sizeof(line), "cam (%.1f, %.1f, %.1f) yaw %.1f pitch %.1f",
           s->camera.pos.x, s->camera.pos.y, s->camera.pos.z,
           s->camera.yaw / DEG2RAD, s->camera.pitch / DEG2RAD);
  y = gfx_text(line, 10, y + 2, 1);

  if (s->show_help) {
    gfx_set_color(180, 220, 255);
    y = gfx_text("move: W/S/A/D, space up, C down", 10, y + 6, 1);
    y = gfx_text("look: I/K pitch, J/L yaw | hold 1 for slow", 10, y + 2, 1);
    y = gfx_text("add line [2], sphere [3], text [4], remove [backspace]", 10, y + 2, 1);
    y = gfx_text("select [ [ ] ] | toggle help [H] | exit: ESC or MENU + X", 10, y + 2, 1);
  }

  if (s->selected >= 0 && s->selected < s->object_count) {
    const w3_object *obj = &s->objects[s->selected];
    gfx_set_color(255, 220, 120);
    if (obj->kind == W3_OBJ_LINE) {
      snprintf(line, sizeof(line), "#%d line A(%.1f %.1f %.1f) B(%.1f %.1f %.1f)",
               s->selected,
               obj->data.line.a.x, obj->data.line.a.y, obj->data.line.a.z,
               obj->data.line.b.x, obj->data.line.b.y, obj->data.line.b.z);
    } else if (obj->kind == W3_OBJ_SPHERE) {
      snprintf(line, sizeof(line), "#%d sphere pos(%.1f %.1f %.1f) r %.2f",
               s->selected,
               obj->data.sphere.pos.x, obj->data.sphere.pos.y, obj->data.sphere.pos.z,
               obj->data.sphere.radius);
    } else {
      snprintf(line, sizeof(line), "#%d text \"%s\" at (%.1f %.1f %.1f)",
               s->selected, obj->data.text.label,
               obj->data.text.pos.x, obj->data.text.pos.y, obj->data.text.pos.z);
    }
    gfx_text(line, 10, y + 6, 1);
  }
}

static void build_basis_scene(w3_state *s) {
  w3_add_line(s, (w3_vec3){-6.0f, 0.0f, 0.0f}, (w3_vec3){6.0f, 0.0f, 0.0f}, 255, 80, 80);
  w3_add_line(s, (w3_vec3){0.0f, -1.5f, 0.0f}, (w3_vec3){0.0f, 5.0f, 0.0f}, 80, 255, 80);
  w3_add_line(s, (w3_vec3){0.0f, 0.0f, -6.0f}, (w3_vec3){0.0f, 0.0f, 6.0f}, 80, 160, 255);

  for (int i = -3; i <= 3; i++) {
    float step = (float)i * 2.0f;
    w3_add_line(s, (w3_vec3){-6.0f, 0.0f, step}, (w3_vec3){6.0f, 0.0f, step}, 60, 60, 60);
    w3_add_line(s, (w3_vec3){step, 0.0f, -6.0f}, (w3_vec3){step, 0.0f, 6.0f}, 60, 60, 60);
  }

  w3_add_sphere_in_front(s, 4.0f, 9.0f);
  w3_add_line_in_front(s, 3.0f, 7.0f);
  w3_add_text_in_front(s, NULL);
}

void w3_init(w3_state *s) {
  memset(s, 0, sizeof(*s));
  s->camera.pos = (w3_vec3){0.0f, 1.5f, 6.0f};
  s->camera.yaw = 0.0f;
  s->camera.pitch = 0.0f;
  s->camera.move_speed = 6.0f;
  s->camera.slow_speed = 2.0f;
  s->camera.look_speed = 1.8f;
  s->show_help = 1;
  s->show_crosshair = 1;
  w3_reset_objects(s);
  build_basis_scene(s);
  s->last_ticks = SDL_GetTicks();
}

void w3_draw(w3_state *s, float fps) {
  uint32_t now = SDL_GetTicks();
  float dt = s->last_ticks ? (now - s->last_ticks) / 1000.0f : 1.0f / FPS;
  s->last_ticks = now;
  if (dt > 0.05f) dt = 0.05f;

  update_camera(s, dt);

  for (int i = 0; i < s->object_count; i++) {
    draw_object(s, &s->objects[i], i == s->selected);
  }

  if (s->show_crosshair) {
    draw_crosshair();
  }
  draw_overlay(s, fps);
}

int w3_handle_key(w3_state *s, char key, int down) {
  if (down && key == BTN_POWER) {
    return 1;
  }

  if (down && key == BTN_MENU) {
    s->menu_combo = 1;
  }
  if (!down && key == BTN_MENU) {
    s->menu_combo = 0;
  }
  if (s->menu_combo && down && key == BTN_X) {
    return 1;
  }

  int pressed = down ? 1 : 0;
  switch (key) {
    case BTN_X:
      s->input.forward = pressed;
      break;
    case 's':
      s->input.backward = pressed;
      break;
    case BTN_B:
      s->input.backward = pressed;
      break;
    case BTN_Y:
      s->input.left = pressed;
      break;
    case BTN_A:
      s->input.right = pressed;
      break;
    case BTN_SELECT:
      s->input.up = pressed;
      break;
    case 'c':
      s->input.down = pressed;
      break;
    case BTN_L1:
      s->input.slow = pressed;
      break;
    case 'j':
    case BTN_LEFT:
      s->input.yaw_left = pressed;
      break;
    case 'l':
    case BTN_RIGHT:
      s->input.yaw_right = pressed;
      break;
    case 'i':
    case BTN_UP:
      s->input.pitch_up = pressed;
      break;
    case 'k':
    case BTN_DOWN:
      s->input.pitch_down = pressed;
      break;
    default:
      break;
  }

  if (!down) {
    return 0;
  }

  if (key == '2' || key == BTN_L2) {
    w3_add_line_in_front(s, 3.0f, 7.0f);
  } else if (key == '3') {
    w3_add_sphere_in_front(s, 4.0f, 9.0f);
  } else if (key == '4') {
    w3_add_text_in_front(s, NULL);
  } else if (key == 8 || key == BTN_R1) {
    w3_remove_selected(s);
  } else if (key == '[') {
    w3_cycle_selection(s, -1);
  } else if (key == ']') {
    w3_cycle_selection(s, 1);
  } else if (key == 'h' || key == 'H') {
    s->show_help = !s->show_help;
  }

  return 0;
}
