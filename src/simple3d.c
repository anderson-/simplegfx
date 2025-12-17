#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <simple3d.h>

#define DEG2RAD (M_PI / 180.0f)

static s3d_state _state;
static s3d_state *s = NULL;

#ifdef USE_SDL2
static int s3d_r = 255, s3d_g = 255, s3d_b = 255;
#define s3d_set_col(r,g,b) do { s3d_r=(r); s3d_g=(g); s3d_b=(b); SDL_SetRenderDrawColor(renderer,(r),(g),(b),255); } while(0)
#define s3d_pixel(x,y) SDL_RenderDrawPoint(renderer,(x),(y))
#define s3d_hline(x1,x2,y) SDL_RenderDrawLine(renderer,(x1),(y),(x2),(y))
#else
static Uint32 s3d_col = 0;
#define s3d_set_col(r,g,b) do { s3d_col = SDL_MapRGB(screen->format,(r),(g),(b)); } while(0)
#define s3d_pixel(x,y) do { if((x)>=0&&(x)<WINDOW_WIDTH&&(y)>=0&&(y)<WINDOW_HEIGHT) ((Uint32*)screen->pixels)[(y)*screen->w+(x)]=s3d_col; } while(0)
#define s3d_hline(x1,x2,y) do { for(int _x=(x1);_x<=(x2);_x++) s3d_pixel(_x,(y)); } while(0)
#endif

s3d_state* s3d_get_state(void) { return s; }


s3d_vec3 s3d_vec(float x, float y, float z) {
  s3d_vec3 v = {x, y, z};
  return v;
}

s3d_vec3 s3d_add(s3d_vec3 a, s3d_vec3 b) {
  return s3d_vec(a.x + b.x, a.y + b.y, a.z + b.z);
}

s3d_vec3 s3d_sub(s3d_vec3 a, s3d_vec3 b) {
  return s3d_vec(a.x - b.x, a.y - b.y, a.z - b.z);
}

s3d_vec3 s3d_scale(s3d_vec3 v, float sc) {
  return s3d_vec(v.x * sc, v.y * sc, v.z * sc);
}

float s3d_len(s3d_vec3 v) {
  return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

s3d_vec3 s3d_norm(s3d_vec3 v) {
  float l = s3d_len(v);
  if (l < 0.0001f) return v;
  return s3d_scale(v, 1.0f / l);
}

s3d_vec3 s3d_cross(s3d_vec3 a, s3d_vec3 b) {
  return s3d_vec(
    a.y * b.z - a.z * b.y,
    a.z * b.x - a.x * b.z,
    a.x * b.y - a.y * b.x
  );
}

float s3d_dot(s3d_vec3 a, s3d_vec3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

static s3d_vec3 cam_forward_internal(void) {
  if (!s) return s3d_vec(0, 0, 1);
  float cy = cosf(s->cam.yaw);
  float sy = sinf(s->cam.yaw);
  float cp = cosf(s->cam.pitch);
  float sp = sinf(s->cam.pitch);
  return s3d_norm(s3d_vec(sy * cp, sp, cy * cp));
}

static s3d_vec3 cam_right_internal(void) {
  if (!s) return s3d_vec(1, 0, 0);
  float cy = cosf(s->cam.yaw);
  float sy = sinf(s->cam.yaw);
  return s3d_norm(s3d_vec(cy, 0.0f, -sy));
}

s3d_vec3 s3d_forward(void) {
  return cam_forward_internal();
}

static int push_obj(s3d_object obj) {
  if (!s || s->obj_count >= S3D_MAX_OBJECTS) return -1;
  s->objects[s->obj_count] = obj;
  return s->obj_count++;
}

void s3d_clear_objs(void) {
  if (!s) return;
  s->obj_count = 0;
  s->selected = -1;
  s->selected_group = -1;
  s->label_counter = 0;
}

int s3d_add_line(s3d_vec3 a, s3d_vec3 b, int r, int g, int bl) {
  return s3d_add_line_ex(a, b, r, g, bl, 1);
}

int s3d_add_line_ex(s3d_vec3 a, s3d_vec3 b, int r, int g, int bl, int thickness) {
  s3d_object obj;
  memset(&obj, 0, sizeof(obj));
  obj.type = S3D_LINE;
  obj.r = r; obj.g = g; obj.b = bl;
  obj.selectable = 1;
  obj.visible = 1;
  obj.group_id = -1;
  obj.data.line.a = a;
  obj.data.line.b = b;
  obj.data.line.thickness = thickness > 0 ? thickness : 1;
  return push_obj(obj);
}

int s3d_add_circle(s3d_vec3 pos, float radius, int r, int g, int bl, int filled) {
  s3d_object obj;
  memset(&obj, 0, sizeof(obj));
  obj.type = S3D_CIRCLE;
  obj.r = r; obj.g = g; obj.b = bl;
  obj.selectable = 1;
  obj.visible = 1;
  obj.group_id = -1;
  obj.data.circle.pos = pos;
  obj.data.circle.radius = radius;
  obj.data.circle.filled = filled;
  return push_obj(obj);
}

int s3d_add_text(s3d_vec3 pos, const char *label, int r, int g, int bl, int size) {
  if (!s) return -1;
  s3d_object obj;
  memset(&obj, 0, sizeof(obj));
  obj.type = S3D_TEXT;
  obj.r = r; obj.g = g; obj.b = bl;
  obj.selectable = 0;
  obj.visible = 1;
  obj.group_id = -1;
  obj.data.text.pos = pos;
  obj.data.text.size = size > 0 ? size : 1;
  if (label) {
    strncpy(obj.data.text.label, label, sizeof(obj.data.text.label) - 1);
  } else {
    snprintf(obj.data.text.label, sizeof(obj.data.text.label), "text %d", ++s->label_counter);
  }
  return push_obj(obj);
}

int s3d_add_point(s3d_vec3 pos, int r, int g, int bl) {
  s3d_object obj;
  memset(&obj, 0, sizeof(obj));
  obj.type = S3D_POINT;
  obj.r = r; obj.g = g; obj.b = bl;
  obj.selectable = 1;
  obj.visible = 1;
  obj.group_id = -1;
  obj.data.point.pos = pos;
  return push_obj(obj);
}

int s3d_add_tri(s3d_vec3 a, s3d_vec3 b, s3d_vec3 c, int r, int g, int bl) {
  s3d_object obj;
  memset(&obj, 0, sizeof(obj));
  obj.type = S3D_TRIANGLE;
  obj.r = r; obj.g = g; obj.b = bl;
  obj.selectable = 1;
  obj.visible = 1;
  obj.group_id = -1;
  obj.data.tri.a = a;
  obj.data.tri.b = b;
  obj.data.tri.c = c;
  return push_obj(obj);
}

void s3d_remove_obj(int idx) {
  if (!s || idx < 0 || idx >= s->obj_count) return;
  int grp = s->objects[idx].group_id;
  if (grp >= 0) {
    for (int i = s->obj_count - 1; i >= 0; i--) {
      if (s->objects[i].group_id == grp) {
        for (int j = i; j < s->obj_count - 1; j++) {
          s->objects[j] = s->objects[j + 1];
        }
        s->obj_count--;
      }
    }
  } else {
    for (int i = idx; i < s->obj_count - 1; i++) {
      s->objects[i] = s->objects[i + 1];
    }
    s->obj_count--;
  }
  if (s->selected >= s->obj_count) s->selected = s->obj_count - 1;
  if (s->obj_count == 0) s->selected = -1;
  s->selected_group = -1;
}

void s3d_set_selectable(int idx, int selectable) {
  if (s && idx >= 0 && idx < s->obj_count) {
    s->objects[idx].selectable = selectable ? 1 : 0;
  }
}

void s3d_set_visible(int idx, int visible) {
  if (s && idx >= 0 && idx < s->obj_count) {
    s->objects[idx].visible = visible ? 1 : 0;
  }
}

void s3d_set_group(int idx, int group_id) {
  if (s && idx >= 0 && idx < s->obj_count) {
    s->objects[idx].group_id = group_id;
  }
}

void s3d_set_color(int idx, int r, int g, int bl) {
  if (s && idx >= 0 && idx < s->obj_count) {
    s->objects[idx].r = r;
    s->objects[idx].g = g;
    s->objects[idx].b = bl;
  }
}

int s3d_new_group(void) {
  if (!s || s->group_count >= S3D_MAX_GROUPS) return -1;
  int id = s->group_count++;
  s->groups[id].id = id;
  s->groups[id].active = 1;
  s->groups[id].offset = s3d_vec(0, 0, 0);
  s->groups[id].rot_x = 0;
  s->groups[id].rot_y = 0;
  s->groups[id].rot_z = 0;
  return id;
}

void s3d_move_group(int group_id, s3d_vec3 offset) {
  if (!s || group_id < 0 || group_id >= s->group_count) return;
  s->groups[group_id].offset = s3d_add(s->groups[group_id].offset, offset);
}

void s3d_rotate_group(int group_id, float rx, float ry, float rz) {
  if (!s || group_id < 0 || group_id >= s->group_count) return;
  s->groups[group_id].rot_x += rx;
  s->groups[group_id].rot_y += ry;
  s->groups[group_id].rot_z += rz;
}

void s3d_delete_group(int group_id) {
  if (!s || group_id < 0 || group_id >= s->group_count) return;
  s->groups[group_id].active = 0;
  for (int i = 0; i < s->obj_count; i++) {
    if (s->objects[i].group_id == group_id) {
      s->objects[i].group_id = -1;
    }
  }
}

void s3d_select_obj(int idx) {
  if (!s) return;
  if (idx >= -1 && idx < s->obj_count) {
    s->selected = idx;
    if (idx >= 0) {
      s->selected_group = s->objects[idx].group_id;
    } else {
      s->selected_group = -1;
    }
  }
}

void s3d_select_next(void) {
  if (!s || s->obj_count == 0) { if (s) s->selected = -1; return; }
  int start = s->selected < 0 ? 0 : s->selected + 1;
  for (int i = 0; i < s->obj_count; i++) {
    int idx = (start + i) % s->obj_count;
    if (s->objects[idx].selectable && s->objects[idx].visible) {
      s3d_select_obj(idx);
      return;
    }
  }
}

void s3d_select_prev(void) {
  if (!s || s->obj_count == 0) { if (s) s->selected = -1; return; }
  int start = s->selected <= 0 ? s->obj_count - 1 : s->selected - 1;
  for (int i = 0; i < s->obj_count; i++) {
    int idx = (start - i + s->obj_count) % s->obj_count;
    if (s->objects[idx].selectable && s->objects[idx].visible) {
      s3d_select_obj(idx);
      return;
    }
  }
}

int s3d_get_selected(void) {
  return s ? s->selected : -1;
}

int s3d_get_hover(void) {
  return s ? s->hover_obj : -1;
}

void s3d_set_mode(s3d_camera_mode mode) {
  if (s) s->cam.mode = mode;
}

s3d_camera_mode s3d_get_mode(void) {
  return s ? s->cam.mode : S3D_MODE_FPS;
}

void s3d_toggle_mode(void) {
  if (s) s->cam.mode = (s->cam.mode == S3D_MODE_FPS) ? S3D_MODE_ORBIT : S3D_MODE_FPS;
}

void s3d_set_cam_pos(s3d_vec3 pos) {
  if (s) s->cam.pos = pos;
}

void s3d_set_cam_target(s3d_vec3 target) {
  if (s) s->cam.target = target;
}

s3d_vec3 s3d_get_cam_pos(void) {
  return s ? s->cam.pos : s3d_vec(0, 0, 0);
}

void s3d_show_grid(int show) {
  if (s) s->show_grid = show ? 1 : 0;
}

void s3d_show_origin(int show) {
  if (s) s->show_origin = show ? 1 : 0;
}

void s3d_show_crosshair(int show) {
  if (s) s->show_crosshair = show ? 1 : 0;
}

void s3d_show_labels(int show) {
  if (s) s->show_labels = show ? 1 : 0;
}

void s3d_debug_pick(int enable) {
  if (!s) return;
  s->debug_pick = enable ? 1 : 0;
  if (enable) {
    s->debug_pick_until = SDL_GetTicks() + 1000;
  }
}

s3d_vec3 s3d_placement_pos(float fixed_dist) {
  if (!s) return s3d_vec(0, 0, 0);
  s3d_vec3 cam = s->cam.pos;
  s3d_vec3 fwd = cam_forward_internal();
  
  if (fixed_dist > 0) {
    return s3d_add(cam, s3d_scale(fwd, fixed_dist));
  }
  
  if (fwd.y < -0.01f) {
    float t = -cam.y / fwd.y;
    if (t > 0.5f && t < 100.0f) {
      return s3d_add(cam, s3d_scale(fwd, t));
    }
  }
  
  return s3d_add(cam, s3d_scale(fwd, 8.0f));
}

void s3d_set_mouse(int x, int y) {
  if (s) { s->mouse_x = x; s->mouse_y = y; }
}

static int project_pt(s3d_vec3 p, int *sx, int *sy, float *depth) {
  if (!s) return 0;
  s3d_vec3 rel = s3d_sub(p, s->cam.pos);
  
  float cy = cosf(s->cam.yaw);
  float syaw = sinf(s->cam.yaw);
  float cp = cosf(s->cam.pitch);
  float sp = sinf(s->cam.pitch);
  
  float dx = rel.x * cy - rel.z * syaw;
  float dz = rel.x * syaw + rel.z * cy;
  float dy = rel.y;
  float dy2 = dy * cp - dz * sp;
  float dz2 = dy * sp + dz * cp;
  
  if (dz2 < 0.01f) dz2 = 0.01f;
  
  float scale = (0.5f * WINDOW_HEIGHT) / tanf(S3D_FOV_DEG * DEG2RAD * 0.5f);
  *sx = (int)(WINDOW_WIDTH / 2 + (dx * scale) / dz2);
  *sy = (int)(WINDOW_HEIGHT / 2 - (dy2 * scale) / dz2);
  if (depth) *depth = dz2;
  return 1;
}

static void draw_line2d(int x0, int y0, int x1, int y1) {
#ifdef USE_SDL2
  SDL_RenderDrawLine(renderer, x0, y0, x1, y1);
#else
  int dx = abs(x1 - x0);
  int sxx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0);
  int syy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  while (1) {
    s3d_pixel(x0, y0);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sxx; }
    if (e2 <= dx) { err += dx; y0 += syy; }
  }
#endif
}

static void draw_circle2d(int cx, int cy, int r, int filled) {
  if (filled) {
    for (int dy = -r; dy <= r; dy++) {
      int dx = (int)sqrtf((float)(r * r - dy * dy));
      int y = cy + dy;
      if (y >= 0 && y < WINDOW_HEIGHT) {
        int x1 = cx - dx, x2 = cx + dx;
        if (x1 < 0) x1 = 0;
        if (x2 >= WINDOW_WIDTH) x2 = WINDOW_WIDTH - 1;
        s3d_hline(x1, x2, y);
      }
    }
  } else {
    int x = r, y = 0, err = 0;
    while (x >= y) {
      s3d_pixel(cx + x, cy + y);
      s3d_pixel(cx + y, cy + x);
      s3d_pixel(cx - y, cy + x);
      s3d_pixel(cx - x, cy + y);
      s3d_pixel(cx - x, cy - y);
      s3d_pixel(cx - y, cy - x);
      s3d_pixel(cx + y, cy - x);
      s3d_pixel(cx + x, cy - y);
      y++;
      err += 1 + 2 * y;
      if (2 * (err - x) + 1 > 0) { x--; err += 1 - 2 * x; }
    }
  }
}

static void draw_tri2d(int x0, int y0, int x1, int y1, int x2, int y2) {
  if (y0 > y1) { int t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }
  if (y0 > y2) { int t = y0; y0 = y2; y2 = t; t = x0; x0 = x2; x2 = t; }
  if (y1 > y2) { int t = y1; y1 = y2; y2 = t; t = x1; x1 = x2; x2 = t; }
  
  if (y2 == y0) return;
  
  for (int y = y0; y <= y2; y++) {
    if (y < 0 || y >= WINDOW_HEIGHT) continue;
    int xa, xb;
    if (y < y1) {
      if (y1 == y0) xa = x0; else xa = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
      xb = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
    } else {
      if (y2 == y1) xa = x1; else xa = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
      xb = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
    }
    if (xa > xb) { int t = xa; xa = xb; xb = t; }
    if (xa < 0) xa = 0;
    if (xb >= WINDOW_WIDTH) xb = WINDOW_WIDTH - 1;
    s3d_hline(xa, xb, y);
  }
}

static float calc_tri_lighting(s3d_vec3 a, s3d_vec3 b, s3d_vec3 c) {
  s3d_vec3 e1 = s3d_sub(b, a);
  s3d_vec3 e2 = s3d_sub(c, a);
  s3d_vec3 normal = s3d_norm(s3d_cross(e1, e2));
  s3d_vec3 light_dir = s3d_norm(s3d_vec(0.3f, 0.8f, -0.5f));
  float ndotl = s3d_dot(normal, light_dir);
  float ambient = 0.3f;
  float diffuse = 0.7f * (ndotl > 0 ? ndotl : -ndotl);
  float intensity = ambient + diffuse;
  if (intensity > 1.0f) intensity = 1.0f;
  return intensity;
}

static s3d_vec3 apply_group_transform(s3d_vec3 p, int group_id) {
  if (!s || group_id < 0 || group_id >= s->group_count) return p;
  s3d_group *g = &s->groups[group_id];
  if (!g->active) return p;
  
  float cx = cosf(g->rot_x), sx = sinf(g->rot_x);
  float cy = cosf(g->rot_y), sy = sinf(g->rot_y);
  float cz = cosf(g->rot_z), sz = sinf(g->rot_z);
  
  float y1 = p.y * cx - p.z * sx;
  float z1 = p.y * sx + p.z * cx;
  
  float x2 = p.x * cy + z1 * sy;
  float z2 = -p.x * sy + z1 * cy;
  
  float x3 = x2 * cz - y1 * sz;
  float y3 = x2 * sz + y1 * cz;
  
  return s3d_add(s3d_vec(x3, y3, z2), g->offset);
}

static Uint32 id_to_color32(int id) {
  id = id + 1;
  int r = (id & 0xFF);
  int g = ((id >> 8) & 0xFF);
  int b = ((id >> 16) & 0xFF);
  return (Uint32)((r << 16) | (g << 8) | b);
}

static int color32_to_id(Uint32 c) {
  int r = (c >> 16) & 0xFF;
  int g = (c >> 8) & 0xFF;
  int b = c & 0xFF;
  int id = r | (g << 8) | (b << 16);
  return id - 1;
}

static void pick_clear(void) {
  if (!s || !s->pick_buffer) return;
  memset(s->pick_buffer, 0, WINDOW_WIDTH * WINDOW_HEIGHT * sizeof(Uint32));
}

static void pick_pixel(int x, int y, Uint32 color) {
  if (!s || !s->pick_buffer) return;
  if (x < 0 || x >= WINDOW_WIDTH || y < 0 || y >= WINDOW_HEIGHT) return;
  s->pick_buffer[y * WINDOW_WIDTH + x] = color;
}

static Uint32 pick_read(int x, int y) {
  if (!s || !s->pick_buffer) return 0;
  if (x < 0 || x >= WINDOW_WIDTH || y < 0 || y >= WINDOW_HEIGHT) return 0;
  return s->pick_buffer[y * WINDOW_WIDTH + x];
}

static void pick_line(int x0, int y0, int x1, int y1, Uint32 color) {
  int dx = abs(x1 - x0);
  int sxx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0);
  int syy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  while (1) {
    pick_pixel(x0, y0, color);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sxx; }
    if (e2 <= dx) { err += dx; y0 += syy; }
  }
}

static void pick_thick_line(int x0, int y0, int x1, int y1, int thickness, Uint32 color) {
  for (int dy = -thickness; dy <= thickness; dy++) {
    for (int dx = -thickness; dx <= thickness; dx++) {
      pick_line(x0 + dx, y0 + dy, x1 + dx, y1 + dy, color);
    }
  }
}

static void pick_fill_rect(int x, int y, int w, int h, Uint32 color) {
  for (int py = y; py < y + h; py++) {
    for (int px = x; px < x + w; px++) {
      pick_pixel(px, py, color);
    }
  }
}

static void pick_fill_circle(int cx, int cy, int radius, Uint32 color) {
  for (int dy = -radius; dy <= radius; dy++) {
    int dx = (int)sqrtf((float)(radius * radius - dy * dy));
    int py = cy + dy;
    for (int px = cx - dx; px <= cx + dx; px++) {
      pick_pixel(px, py, color);
    }
  }
}

static void pick_fill_tri(int x0, int y0, int x1, int y1, int x2, int y2, Uint32 color) {
  if (y0 > y1) { int t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }
  if (y0 > y2) { int t = y0; y0 = y2; y2 = t; t = x0; x0 = x2; x2 = t; }
  if (y1 > y2) { int t = y1; y1 = y2; y2 = t; t = x1; x1 = x2; x2 = t; }
  if (y2 == y0) return;
  for (int y = y0; y <= y2; y++) {
    int xa, xb;
    if (y < y1) {
      if (y1 == y0) xa = x0; else xa = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
      xb = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
    } else {
      if (y2 == y1) xa = x1; else xa = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
      xb = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
    }
    if (xa > xb) { int t = xa; xa = xb; xb = t; }
    for (int x = xa; x <= xb; x++) pick_pixel(x, y, color);
  }
}

static void pick_fill_convex(int *px, int *py, int n, Uint32 color) {
  if (n < 3) return;
  int cx = 0, cy = 0;
  for (int i = 0; i < n; i++) { cx += px[i]; cy += py[i]; }
  cx /= n; cy /= n;
  for (int i = 0; i < n; i++) {
    int j = (i + 1) % n;
    pick_fill_tri(cx, cy, px[i], py[i], px[j], py[j], color);
  }
}

static void draw_group_to_pick_solid(int group_id) {
  if (!s || group_id < 0) return;
  Uint32 color = id_to_color32(group_id + S3D_MAX_OBJECTS);
  
  int px[256], py[256];
  int pt_count = 0;
  
  for (int i = 0; i < s->obj_count && pt_count < 254; i++) {
    s3d_object *obj = &s->objects[i];
    if (obj->group_id != group_id || !obj->visible) continue;
    
    if (obj->type == S3D_LINE) {
      s3d_vec3 a = apply_group_transform(obj->data.line.a, group_id);
      s3d_vec3 b = apply_group_transform(obj->data.line.b, group_id);
      int x0, y0, x1, y1;
      float d0, d1;
      if (project_pt(a, &x0, &y0, &d0) && pt_count < 256) {
        px[pt_count] = x0; py[pt_count] = y0; pt_count++;
      }
      if (project_pt(b, &x1, &y1, &d1) && pt_count < 256) {
        px[pt_count] = x1; py[pt_count] = y1; pt_count++;
      }
    } else if (obj->type == S3D_CIRCLE) {
      s3d_vec3 pos = apply_group_transform(obj->data.circle.pos, group_id);
      int sx, sy; float d;
      if (project_pt(pos, &sx, &sy, &d)) {
        float scale = (0.5f * WINDOW_HEIGHT) / tanf(S3D_FOV_DEG * DEG2RAD * 0.5f);
        int r = (int)((obj->data.circle.radius * scale) / d);
        if (r < 1) r = 1; if (r > 500) r = 500;
        pick_fill_circle(sx, sy, r + 4, color);
      }
    } else if (obj->type == S3D_TRIANGLE) {
      s3d_vec3 a = apply_group_transform(obj->data.tri.a, group_id);
      s3d_vec3 b = apply_group_transform(obj->data.tri.b, group_id);
      s3d_vec3 c = apply_group_transform(obj->data.tri.c, group_id);
      int x0, y0, x1, y1, x2, y2;
      float d0, d1, d2;
      project_pt(a, &x0, &y0, &d0);
      project_pt(b, &x1, &y1, &d1);
      project_pt(c, &x2, &y2, &d2);
      pick_fill_tri(x0, y0, x1, y1, x2, y2, color);
    } else if (obj->type == S3D_POINT) {
      s3d_vec3 pos = apply_group_transform(obj->data.point.pos, group_id);
      int sx, sy; float d;
      if (project_pt(pos, &sx, &sy, &d) && pt_count < 256) {
        px[pt_count] = sx; py[pt_count] = sy; pt_count++;
      }
    }
  }
  
  if (pt_count >= 3) {
    int cx = 0, cy = 0;
    for (int i = 0; i < pt_count; i++) { cx += px[i]; cy += py[i]; }
    cx /= pt_count; cy /= pt_count;
    
    for (int i = 0; i < pt_count - 1; i++) {
      for (int j = i + 1; j < pt_count; j++) {
        float ai = atan2f((float)(py[i] - cy), (float)(px[i] - cx));
        float aj = atan2f((float)(py[j] - cy), (float)(px[j] - cx));
        if (aj < ai) {
          int t = px[i]; px[i] = px[j]; px[j] = t;
          t = py[i]; py[i] = py[j]; py[j] = t;
        }
      }
    }
    pick_fill_convex(px, py, pt_count, color);
  }
}

static void draw_obj_to_pick_buffer(s3d_object *obj, int obj_idx) {
  if (!s) return;
  
  if (obj->group_id < 0 && !obj->selectable) return;
  
  int pick_id = obj_idx;
  if (obj->group_id >= 0) {
    pick_id = obj->group_id + S3D_MAX_OBJECTS;
  }
  
  Uint32 color = id_to_color32(pick_id);
  
  if (obj->type == S3D_LINE) {
    s3d_vec3 a = apply_group_transform(obj->data.line.a, obj->group_id);
    s3d_vec3 b = apply_group_transform(obj->data.line.b, obj->group_id);
    int x0, y0, x1, y1;
    float d0, d1;
    if (project_pt(a, &x0, &y0, &d0) && project_pt(b, &x1, &y1, &d1)) {
      pick_thick_line(x0, y0, x1, y1, 4, color);
    }
  } else if (obj->type == S3D_CIRCLE) {
    s3d_vec3 pos = apply_group_transform(obj->data.circle.pos, obj->group_id);
    int sxx, syy;
    float depth;
    if (project_pt(pos, &sxx, &syy, &depth)) {
      float scale = (0.5f * WINDOW_HEIGHT) / tanf(S3D_FOV_DEG * DEG2RAD * 0.5f);
      int radius = (int)((obj->data.circle.radius * scale) / depth);
      if (radius < 1) radius = 1;
      if (radius > 500) radius = 500;
      pick_fill_circle(sxx, syy, radius + 4, color);
    }
  } else if (obj->type == S3D_POINT) {
    s3d_vec3 pos = apply_group_transform(obj->data.point.pos, obj->group_id);
    int sxx, syy;
    float depth;
    if (project_pt(pos, &sxx, &syy, &depth)) {
      pick_fill_rect(sxx - 5, syy - 5, 11, 11, color);
    }
  } else if (obj->type == S3D_TRIANGLE) {
    s3d_vec3 a = apply_group_transform(obj->data.tri.a, obj->group_id);
    s3d_vec3 b = apply_group_transform(obj->data.tri.b, obj->group_id);
    s3d_vec3 c = apply_group_transform(obj->data.tri.c, obj->group_id);
    int x0, y0, x1, y1, x2, y2;
    float d0, d1, d2;
    project_pt(a, &x0, &y0, &d0);
    project_pt(b, &x1, &y1, &d1);
    project_pt(c, &x2, &y2, &d2);
    pick_fill_tri(x0, y0, x1, y1, x2, y2, color);
  }
}

static void draw_obj(s3d_object *obj, int highlighted, int hovered) {
  if (!s) return;
  int cr = obj->r, cg = obj->g, cb = obj->b;
  
  if (highlighted) {
    cr = s->select_r; cg = s->select_g; cb = s->select_b;
  } else if (hovered) {
    cr = s->hover_r; cg = s->hover_g; cb = s->hover_b;
  }
  
  s3d_set_col(cr, cg, cb);
  
  if (obj->type == S3D_LINE) {
    s3d_vec3 a = apply_group_transform(obj->data.line.a, obj->group_id);
    s3d_vec3 b = apply_group_transform(obj->data.line.b, obj->group_id);
    int x0, y0, x1, y1;
    float d0, d1;
    project_pt(a, &x0, &y0, &d0);
    project_pt(b, &x1, &y1, &d1);
    int thick = obj->data.line.thickness;
    if (thick <= 1) {
      draw_line2d(x0, y0, x1, y1);
    } else {
      for (int dy = -thick/2; dy <= thick/2; dy++) {
        for (int dx = -thick/2; dx <= thick/2; dx++) {
          draw_line2d(x0+dx, y0+dy, x1+dx, y1+dy);
        }
      }
    }
  } else if (obj->type == S3D_CIRCLE) {
    s3d_vec3 pos = apply_group_transform(obj->data.circle.pos, obj->group_id);
    int sxx, syy;
    float depth;
    project_pt(pos, &sxx, &syy, &depth);
    float scale = (0.5f * WINDOW_HEIGHT) / tanf(S3D_FOV_DEG * DEG2RAD * 0.5f);
    int radius = (int)((obj->data.circle.radius * scale) / depth);
    if (radius < 1) radius = 1;
    if (radius > 500) radius = 500;
    draw_circle2d(sxx, syy, radius, obj->data.circle.filled);
  } else if (obj->type == S3D_TEXT) {
    if (!s->show_labels) return;
    s3d_vec3 pos = apply_group_transform(obj->data.text.pos, obj->group_id);
    int sxx, syy;
    float depth;
    project_pt(pos, &sxx, &syy, &depth);
    gfx_text(obj->data.text.label, sxx, syy, obj->data.text.size);
  } else if (obj->type == S3D_POINT) {
    s3d_vec3 pos = apply_group_transform(obj->data.point.pos, obj->group_id);
    int sxx, syy;
    float depth;
    project_pt(pos, &sxx, &syy, &depth);
    gfx_fill_rect(sxx - 1, syy - 1, 3, 3);
  } else if (obj->type == S3D_TRIANGLE) {
    s3d_vec3 a = apply_group_transform(obj->data.tri.a, obj->group_id);
    s3d_vec3 b = apply_group_transform(obj->data.tri.b, obj->group_id);
    s3d_vec3 c = apply_group_transform(obj->data.tri.c, obj->group_id);
    int x0, y0, x1, y1, x2, y2;
    float d0, d1, d2;
    project_pt(a, &x0, &y0, &d0);
    project_pt(b, &x1, &y1, &d1);
    project_pt(c, &x2, &y2, &d2);
    if (!highlighted && !hovered) {
      float lit = calc_tri_lighting(a, b, c);
      int lr = (int)(cr * lit); if (lr > 255) lr = 255;
      int lg = (int)(cg * lit); if (lg > 255) lg = 255;
      int lb = (int)(cb * lit); if (lb > 255) lb = 255;
      s3d_set_col(lr, lg, lb);
    }
    draw_tri2d(x0, y0, x1, y1, x2, y2);
  }
}

static void draw_line3d(s3d_vec3 a, s3d_vec3 b) {
  int x0, y0, x1, y1;
  float d0, d1;
  project_pt(a, &x0, &y0, &d0);
  project_pt(b, &x1, &y1, &d1);
  draw_line2d(x0, y0, x1, y1);
}

static void draw_grid(void) {
  if (!s) return;
  s3d_set_col(50, 50, 50);
  for (int i = -10; i <= 10; i++) {
    float step = (float)i;
    draw_line3d(s3d_vec(-10.0f, 0.0f, step), s3d_vec(10.0f, 0.0f, step));
    draw_line3d(s3d_vec(step, 0.0f, -10.0f), s3d_vec(step, 0.0f, 10.0f));
  }
}

static void draw_origin(void) {
  if (!s) return;
  s3d_vec3 o = s3d_vec(0, 0, 0);
  s3d_set_col(255, 60, 60);
  draw_line3d(o, s3d_vec(1, 0, 0));
  s3d_set_col(60, 255, 60);
  draw_line3d(o, s3d_vec(0, 1, 0));
  s3d_set_col(60, 60, 255);
  draw_line3d(o, s3d_vec(0, 0, 1));
}

static void draw_crosshair(void) {
  int cx = WINDOW_WIDTH / 2;
  int cy = WINDOW_HEIGHT / 2;
  s3d_set_col(255, 255, 255);
  draw_line2d(cx - 6, cy, cx + 6, cy);
  draw_line2d(cx, cy - 6, cx, cy + 6);
}

static void update_cam_fps(float dt) {
  if (!s) return;
  float speed = s->input.slow ? s->cam.slow_speed : s->cam.move_speed;
  s3d_vec3 fwd = cam_forward_internal();
  s3d_vec3 right = cam_right_internal();
  s3d_vec3 move = s3d_vec(0, 0, 0);
  
  if (s->input.forward) move = s3d_add(move, fwd);
  if (s->input.backward) move = s3d_sub(move, fwd);
  if (s->input.left) move = s3d_sub(move, right);
  if (s->input.right) move = s3d_add(move, right);
  if (s->input.up) move.y += 1.0f;
  if (s->input.down) move.y -= 1.0f;
  
  float len = s3d_len(move);
  if (len > 0.001f) {
    move = s3d_scale(move, 1.0f / len);
    s->cam.pos = s3d_add(s->cam.pos, s3d_scale(move, speed * dt));
  }
  
  s->cam.yaw += (s->input.yaw_right - s->input.yaw_left) * s->cam.look_speed * dt;
  s->cam.pitch += (s->input.pitch_up - s->input.pitch_down) * s->cam.look_speed * dt;
  float limit = DEG2RAD * 89.0f;
  if (s->cam.pitch > limit) s->cam.pitch = limit;
  if (s->cam.pitch < -limit) s->cam.pitch = -limit;
}

static void update_cam_orbit(float dt) {
  if (!s) return;
  s->cam.yaw += (s->input.yaw_left - s->input.yaw_right) * s->cam.look_speed * dt;
  s->cam.pitch += (s->input.pitch_down - s->input.pitch_up) * s->cam.look_speed * dt;
  
  float limit = DEG2RAD * 85.0f;
  if (s->cam.pitch > limit) s->cam.pitch = limit;
  if (s->cam.pitch < -limit) s->cam.pitch = -limit;
  
  float zoom = (s->input.zoom_out - s->input.zoom_in) * s->cam.zoom_speed * dt;
  s->cam.distance += zoom;
  if (s->cam.distance < 2.0f) s->cam.distance = 2.0f;
  if (s->cam.distance > 100.0f) s->cam.distance = 100.0f;
  
  float cy = cosf(s->cam.yaw);
  float sy = sinf(s->cam.yaw);
  float cp = cosf(s->cam.pitch);
  float sp = sinf(s->cam.pitch);
  
  s->cam.pos.x = s->cam.target.x - sy * cp * s->cam.distance;
  s->cam.pos.y = s->cam.target.y + sp * s->cam.distance;
  s->cam.pos.z = s->cam.target.z - cy * cp * s->cam.distance;
}

void s3d_update(float dt) {
  if (!s) return;
  if (dt > 0.05f) dt = 0.05f;
  
  if (s->cam.mode == S3D_MODE_FPS) {
    update_cam_fps(dt);
  } else {
    update_cam_orbit(dt);
  }
  
  s->hover_obj = -1;
}

static float obj_depth(s3d_object *obj);

static float group_depth(int group_id) {
  if (!s || group_id < 0) return 0;
  float min_depth = 1e9f;
  for (int i = 0; i < s->obj_count; i++) {
    if (s->objects[i].group_id == group_id && s->objects[i].visible) {
      float d = obj_depth(&s->objects[i]);
      if (d < min_depth) min_depth = d;
    }
  }
  return min_depth;
}

void s3d_draw_pick(void) {
  if (!s) return;
  pick_clear();
  
  static int order[S3D_MAX_OBJECTS];
  static float depths[S3D_MAX_OBJECTS];
  static int is_group[S3D_MAX_OBJECTS];
  int count = 0;
  
  static int group_done[S3D_MAX_GROUPS];
  memset(group_done, 0, sizeof(group_done));
  
  for (int i = 0; i < s->obj_count; i++) {
    s3d_object *obj = &s->objects[i];
    if (!obj->visible) continue;
    
    if (obj->group_id >= 0 && obj->group_id < S3D_MAX_GROUPS) {
      if (!group_done[obj->group_id]) {
        order[count] = obj->group_id;
        depths[count] = group_depth(obj->group_id);
        is_group[count] = 1;
        count++;
        group_done[obj->group_id] = 1;
      }
    } else {
      order[count] = i;
      depths[count] = obj_depth(obj);
      is_group[count] = 0;
      count++;
    }
  }
  
  for (int i = 0; i < count - 1; i++) {
    for (int j = i + 1; j < count; j++) {
      if (depths[j] > depths[i]) {
        int to = order[i]; order[i] = order[j]; order[j] = to;
        float td = depths[i]; depths[i] = depths[j]; depths[j] = td;
        int tg = is_group[i]; is_group[i] = is_group[j]; is_group[j] = tg;
      }
    }
  }
  
  for (int i = 0; i < count; i++) {
    if (is_group[i]) {
      draw_group_to_pick_solid(order[i]);
    } else {
      draw_obj_to_pick_buffer(&s->objects[order[i]], order[i]);
    }
  }
}

int s3d_pick_at(int mx, int my) {
  if (!s || !s->pick_buffer) return -1;
  s3d_draw_pick();
  Uint32 color = pick_read(mx, my);
  if (color == 0) return -1;
  int pick_id = color32_to_id(color);
  if (pick_id >= S3D_MAX_OBJECTS) {
    int group_id = pick_id - S3D_MAX_OBJECTS;
    for (int i = 0; i < s->obj_count; i++) {
      if (s->objects[i].group_id == group_id && s->objects[i].selectable) {
        return i;
      }
    }
  } else if (pick_id >= 0 && pick_id < s->obj_count) {
    return pick_id;
  }
  return -1;
}

int s3d_do_pick(void) {
  if (!s) return -1;
  s3d_draw_pick();
  
  s3d_debug_pick(1);
  
  int cx = WINDOW_WIDTH / 2;
  int cy = WINDOW_HEIGHT / 2;
  
  Uint32 color = pick_read(cx, cy);
  if (color == 0) {
    s->pick_result = -1;
    return -1;
  }
  
  int pick_id = color32_to_id(color);
  
  if (pick_id >= S3D_MAX_OBJECTS) {
    int group_id = pick_id - S3D_MAX_OBJECTS;
    for (int i = 0; i < s->obj_count; i++) {
      if (s->objects[i].group_id == group_id && s->objects[i].selectable) {
        s->pick_result = i;
        s->selected_group = group_id;
        return i;
      }
    }
  } else if (pick_id >= 0 && pick_id < s->obj_count) {
    s->pick_result = pick_id;
    s->selected_group = s->objects[pick_id].group_id;
    return pick_id;
  }
  
  s->pick_result = -1;
  return -1;
}

static void draw_debug_pick_buffer(void) {
  if (!s || !s->pick_buffer) return;
  for (int y = 0; y < WINDOW_HEIGHT; y++) {
    for (int x = 0; x < WINDOW_WIDTH; x++) {
      Uint32 c = s->pick_buffer[y * WINDOW_WIDTH + x];
      if (c != 0) {
        int r = (c >> 16) & 0xFF;
        int g = (c >> 8) & 0xFF;
        int b = c & 0xFF;
        s3d_set_col(r * 3 % 256, g * 7 % 256, b * 11 % 256);
        s3d_pixel(x, y);
      }
    }
  }
}

static float obj_depth(s3d_object *obj) {
  s3d_vec3 pos;
  if (obj->type == S3D_LINE) {
    s3d_vec3 a = apply_group_transform(obj->data.line.a, obj->group_id);
    s3d_vec3 b = apply_group_transform(obj->data.line.b, obj->group_id);
    pos = s3d_scale(s3d_add(a, b), 0.5f);
  } else if (obj->type == S3D_CIRCLE) {
    pos = apply_group_transform(obj->data.circle.pos, obj->group_id);
  } else if (obj->type == S3D_TEXT) {
    pos = apply_group_transform(obj->data.text.pos, obj->group_id);
  } else if (obj->type == S3D_TRIANGLE) {
    s3d_vec3 a = apply_group_transform(obj->data.tri.a, obj->group_id);
    s3d_vec3 b = apply_group_transform(obj->data.tri.b, obj->group_id);
    s3d_vec3 c = apply_group_transform(obj->data.tri.c, obj->group_id);
    pos = s3d_scale(s3d_add(s3d_add(a, b), c), 1.0f/3.0f);
  } else {
    pos = apply_group_transform(obj->data.point.pos, obj->group_id);
  }
  return s3d_len(s3d_sub(pos, s->cam.pos));
}

void s3d_draw(void) {
  if (!s) return;
  
  if (s->debug_pick && SDL_GetTicks() < s->debug_pick_until) {
    s3d_draw_pick();
    draw_debug_pick_buffer();
    if (s->show_crosshair) draw_crosshair();
    return;
  }
  s->debug_pick = 0;
  
  if (s->show_grid) draw_grid();
  if (s->show_origin) draw_origin();
  
  static int order[S3D_MAX_OBJECTS];
  static float depths[S3D_MAX_OBJECTS];
  int count = 0;
  
  for (int i = 0; i < s->obj_count; i++) {
    if (!s->objects[i].visible) continue;
    order[count] = i;
    depths[count] = obj_depth(&s->objects[i]);
    count++;
  }
  
  for (int i = 0; i < count - 1; i++) {
    for (int j = i + 1; j < count; j++) {
      if (depths[j] > depths[i]) {
        int ti = order[i]; order[i] = order[j]; order[j] = ti;
        float td = depths[i]; depths[i] = depths[j]; depths[j] = td;
      }
    }
  }
  
  for (int i = 0; i < count; i++) {
    int idx = order[i];
    s3d_object *obj = &s->objects[idx];
    
    int highlighted = 0;
    if (s->selected_group >= 0 && obj->group_id == s->selected_group) {
      highlighted = 1;
    } else if (idx == s->selected) {
      highlighted = 1;
    }
    int hovered = (idx == s->hover_obj && !highlighted);
    
    draw_obj(obj, highlighted, hovered);
  }
  
  if (s->show_crosshair) draw_crosshair();
}

void s3d_init(void) {
  s = &_state;
  memset(s, 0, sizeof(*s));
  s->cam.pos = s3d_vec(0.0f, 2.0f, -10.0f);
  s->cam.target = s3d_vec(0.0f, 1.0f, 0.0f);
  s->cam.yaw = 0.0f;
  s->cam.pitch = 0.15f;
  s->cam.distance = 15.0f;
  s->cam.move_speed = 6.0f;
  s->cam.slow_speed = 2.0f;
  s->cam.look_speed = 1.8f;
  s->cam.zoom_speed = 10.0f;
  s->cam.mode = S3D_MODE_FPS;
  
  s->show_grid = 1;
  s->show_origin = 1;
  s->show_crosshair = 1;
  s->show_labels = 1;
  
  s->selected = -1;
  s->selected_group = -1;
  s->hover_obj = -1;
  s->pick_result = -1;
  
  s->select_r = 255; s->select_g = 255; s->select_b = 100;
  s->hover_r = 180; s->hover_g = 220; s->hover_b = 255;
  
  s->pick_buffer = (Uint32*)malloc(WINDOW_WIDTH * WINDOW_HEIGHT * sizeof(Uint32));
  if (s->pick_buffer) {
    memset(s->pick_buffer, 0, WINDOW_WIDTH * WINDOW_HEIGHT * sizeof(Uint32));
  }
  
  s->last_ticks = SDL_GetTicks();
}

int s3d_handle_key(char key, int down) {
  if (!s) return 0;
  if (down && key == BTN_POWER) return 1;
  if (down && key == BTN_MENU) s->menu_combo = 1;
  if (!down && key == BTN_MENU) s->menu_combo = 0;
  if (s->menu_combo && down && key == BTN_X) return 1;
  
  int p = down ? 1 : 0;
  switch (key) {
    case 'w': case BTN_UP: s->input.forward = p; break;
    case 's': case BTN_DOWN: s->input.backward = p; break;
    case 'a': case BTN_LEFT: s->input.left = p; break;
    case 'd': case BTN_RIGHT: s->input.right = p; break;
    case ' ': case BTN_R1: s->input.up = p; break;
    case 'c': case BTN_L1: s->input.down = p; break;
    case BTN_L2: s->input.slow = p; break;
    case 'j': s->input.yaw_left = p; break;
    case 'l': s->input.yaw_right = p; break;
    case 'i': s->input.pitch_up = p; break;
    case 'k': s->input.pitch_down = p; break;
    case 'q': s->input.zoom_in = p; break;
    case 'e': s->input.zoom_out = p; break;
#ifndef FULL_KEYBOARD
    case BTN_Y: s->input.zoom_in = p; break;
    case BTN_A: s->input.zoom_out = p; break;
#endif
    default: break;
  }
  
  if (!down) return 0;
  
  if (key == '[' || key == BTN_X) s3d_select_prev();
  else if (key == ']' || key == BTN_B) s3d_select_next();
  else if (key == 'm' || key == 'M') s3d_toggle_mode();
  
  return 0;
}
