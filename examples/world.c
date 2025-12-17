#include <simplegfx.h>
#include <simple3d.h>
#include <simple3do.h>
#include <stdio.h>

static int show_help = 1;
static int cube_group = -1;
static int pyramid_group = -1;

static void build_scene(void) {
  s3d_clear_objs();
  
  cube_group = s3d_new_group();
  int cube_start = shape_cube(s3d_vec(-3.0f, 1.0f, 0.0f), 1.5f, 100, 180, 255);
  shape_make_selectable(cube_start, cube_group);
  
  pyramid_group = s3d_new_group();
  int pyr_start = shape_pyramid(s3d_vec(3.0f, 0.0f, 0.0f), 1.5f, 2.0f, 255, 150, 100);
  shape_make_selectable(pyr_start, pyramid_group);
  
  int cone_grp = s3d_new_group();
  int cone_start = shape_cone(s3d_vec(0.0f, 0.0f, -4.0f), 0.8f, 1.5f, 12, 150, 255, 150);
  shape_make_selectable(cone_start, cone_grp);
  
  int cyl_grp = s3d_new_group();
  int cyl_start = shape_cylinder(s3d_vec(0.0f, 0.0f, 4.0f), 0.6f, 2.0f, 12, 255, 200, 100);
  shape_make_selectable(cyl_start, cyl_grp);
  
  int sphere_grp = s3d_new_group();
  int sphere_start = shape_sphere_wire(s3d_vec(5.0f, 1.5f, -3.0f), 1.0f, 6, 12, 200, 150, 255);
  shape_make_selectable(sphere_start, sphere_grp);
  
  s3d_add_circle(s3d_vec(-5.0f, 1.0f, 3.0f), 0.8f, 255, 100, 100, 0);
  s3d_add_circle(s3d_vec(-5.0f, 1.0f, 5.0f), 0.6f, 100, 255, 100, 1);
  
  s3d_add_text(s3d_vec(0.0f, 3.0f, 0.0f), "Simple3D Demo", 255, 255, 200, 1);
  s3d_add_text(s3d_vec(-3.0f, 2.5f, 0.0f), "Cube", 180, 220, 255, 1);
  s3d_add_text(s3d_vec(3.0f, 2.5f, 0.0f), "Pyramid", 255, 200, 180, 1);
}

static void draw_overlay(float fps) {
  s3d_state *s = s3d_get_state();
  if (!s) return;
  char line[128];
  int y = 8;
  
  gfx_set_color(255, 255, 255);
  snprintf(line, sizeof(line), "FPS %.1f | Objs %d/%d | Mode: %s",
           fps, s->obj_count, S3D_MAX_OBJECTS,
           s3d_get_mode() == S3D_MODE_FPS ? "FPS" : "ORBIT");
  y = gfx_text(line, 8, y, 1);
  
  s3d_vec3 cam = s3d_get_cam_pos();
  snprintf(line, sizeof(line), "Cam (%.1f, %.1f, %.1f)", cam.x, cam.y, cam.z);
  y = gfx_text(line, 8, y + 2, 1);
  
  if (show_help) {
    gfx_set_color(180, 220, 255);
    y = gfx_text("Move: W/S/A/D, Space/C up/down", 8, y + 6, 1);
    y = gfx_text("Look: I/K/J/L | Zoom(orbit): Q/E", 8, y + 2, 1);
    y = gfx_text("[M] Toggle FPS/Orbit mode", 8, y + 2, 1);
    y = gfx_text("[G] Grid [O] Origin [X] Crosshair [T] Labels", 8, y + 2, 1);
    y = gfx_text("[1-3] Wire shapes [4-5] Solid shapes", 8, y + 2, 1);
    y = gfx_text("[P] Pick at cursor | [Backspace] Remove | [H] Help", 8, y + 2, 1);
    y = gfx_text("[R] Reset scene | ESC Exit", 8, y + 2, 1);
  }
  
  int sel = s3d_get_selected();
  if (sel >= 0) {
    gfx_set_color(255, 220, 120);
    snprintf(line, sizeof(line), "Selected: #%d (group %d)", sel, s->selected_group);
    gfx_text(line, 8, y + 6, 1);
  }
  
  int hover = s3d_get_hover();
  if (hover >= 0 && hover != sel) {
    gfx_set_color(180, 200, 255);
    snprintf(line, sizeof(line), "Hover: #%d", hover);
    gfx_text(line, 8, WINDOW_HEIGHT - 20, 1);
  }
}

void gfx_app(int init) {
  if (init) {
    s3d_init();
    build_scene();
  }
}

void gfx_process_data(int compute_time) {
  (void)compute_time;
}

void gfx_draw(float fps) {
  s3d_state *s = s3d_get_state();
  if (!s) return;
  uint32_t now = SDL_GetTicks();
  float dt = s->last_ticks ? (now - s->last_ticks) / 1000.0f : 1.0f / FPS;
  s->last_ticks = now;
  
  s3d_update(dt);
  s3d_draw();
  draw_overlay(fps);
}

static float frand(float min, float max) {
  float t = (float)rand() / 32767.0f;
  return min + t * (max - min);
}

int gfx_on_key(char key, int down) {
  s3d_state *s = s3d_get_state();
  int ret = s3d_handle_key(key, down);
  if (ret) return ret;
  
  if (!down || !s) return 0;
  
  switch (key) {
    case 'h': case 'H':
      show_help = !show_help;
      break;
    case 'g': case 'G':
      s3d_show_grid(!s->show_grid);
      break;
    case 'o': case 'O':
      s3d_show_origin(!s->show_origin);
      break;
    case 'x': case 'X':
      s3d_show_crosshair(!s->show_crosshair);
      break;
    case 't': case 'T':
      s3d_show_labels(!s->show_labels);
      break;
    case 'r': case 'R':
      build_scene();
      break;
    case '1': {
      s3d_vec3 base = s3d_placement_pos(0);
      s3d_vec3 pos = s3d_vec(base.x, base.y + 0.75f, base.z);
      int grp = s3d_new_group();
      int first = shape_cube(pos, frand(0.8f, 1.5f), 
                 (int)frand(100, 255), (int)frand(100, 255), (int)frand(100, 255));
      shape_make_selectable(first, grp);
      break;
    }
    case '2': {
      s3d_vec3 base = s3d_placement_pos(0);
      s3d_vec3 pos = s3d_vec(base.x, 0.0f, base.z);
      int grp = s3d_new_group();
      int first = shape_pyramid(pos, frand(1.0f, 2.0f), frand(1.5f, 2.5f),
                    (int)frand(100, 255), (int)frand(100, 255), (int)frand(100, 255));
      shape_make_selectable(first, grp);
      break;
    }
    case '3': {
      s3d_vec3 base = s3d_placement_pos(0);
      s3d_vec3 pos = s3d_vec(base.x, 0.0f, base.z);
      int grp = s3d_new_group();
      int first = shape_cone(pos, frand(0.5f, 1.0f), frand(1.0f, 2.0f), 12,
                 (int)frand(100, 255), (int)frand(100, 255), (int)frand(100, 255));
      shape_make_selectable(first, grp);
      break;
    }
    case '4': {
      s3d_vec3 base = s3d_placement_pos(0);
      s3d_vec3 pos = s3d_vec(base.x, base.y + 0.75f, base.z);
      int grp = s3d_new_group();
      int first = shape_cube_solid(pos, frand(0.8f, 1.5f),
                 (int)frand(80, 200), (int)frand(80, 200), (int)frand(80, 200));
      shape_make_selectable(first, grp);
      break;
    }
    case '5': {
      s3d_vec3 base = s3d_placement_pos(0);
      s3d_vec3 pos = s3d_vec(base.x, 0.0f, base.z);
      int grp = s3d_new_group();
      int first = shape_pyramid_solid(pos, frand(1.0f, 2.0f), frand(1.5f, 2.5f),
                 (int)frand(80, 200), (int)frand(80, 200), (int)frand(80, 200));
      shape_make_selectable(first, grp);
      break;
    }
    case 8: {
      int sel = s3d_get_selected();
      if (sel >= 0) s3d_remove_obj(sel);
      break;
    }
    case 'p': case 'P': case 13: {
      int picked = s3d_do_pick();
      if (picked >= 0) {
        s3d_select_obj(picked);
      }
      break;
    }
    default:
      break;
  }
  
  return 0;
}
