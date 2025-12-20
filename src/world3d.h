#ifndef WORLD3D_H
#define WORLD3D_H

#include <simplegfx.h>

#define W3_MAX_OBJECTS 128
#define W3_FOV_DEG 70.0f
#define W3_NEAR_PLANE 0.1f
#define W3_FAR_PLANE 200.0f

typedef struct {
  float x;
  float y;
  float z;
} w3_vec3;

typedef enum {
  W3_OBJ_LINE = 0,
  W3_OBJ_SPHERE,
  W3_OBJ_TEXT
} w3_object_kind;

typedef struct {
  w3_vec3 a;
  w3_vec3 b;
} w3_line;

typedef struct {
  w3_vec3 pos;
  float radius;
} w3_sphere;

typedef struct {
  w3_vec3 pos;
  char label[48];
} w3_text;

typedef struct {
  w3_object_kind kind;
  int r;
  int g;
  int b;
  union {
    w3_line line;
    w3_sphere sphere;
    w3_text text;
  } data;
} w3_object;

typedef struct {
  w3_vec3 pos;
  float yaw;
  float pitch;
  float move_speed;
  float slow_speed;
  float look_speed;
} w3_camera;

typedef struct {
  int forward;
  int backward;
  int left;
  int right;
  int up;
  int down;
  int yaw_left;
  int yaw_right;
  int pitch_up;
  int pitch_down;
  int slow;
} w3_input_state;

typedef struct {
  w3_object objects[W3_MAX_OBJECTS];
  int object_count;
  int selected;
  int label_counter;
  w3_camera camera;
  w3_input_state input;
  uint32_t last_ticks;
  int menu_combo;
  int show_help;
  int show_crosshair;
} w3_state;

void w3_init(w3_state *s);
void w3_reset_objects(w3_state *s);
int w3_add_line(w3_state *s, w3_vec3 a, w3_vec3 b, int r, int g, int bcol);
int w3_add_sphere(w3_state *s, w3_vec3 pos, float radius, int r, int g, int bcol);
int w3_add_text(w3_state *s, w3_vec3 pos, const char *label, int r, int g, int bcol);
int w3_add_line_in_front(w3_state *s, float min_dist, float max_dist);
int w3_add_sphere_in_front(w3_state *s, float min_dist, float max_dist);
int w3_add_text_in_front(w3_state *s, const char *label);
void w3_remove_selected(w3_state *s);
void w3_cycle_selection(w3_state *s, int dir);
int w3_handle_key(w3_state *s, char key, int down);
void w3_draw(w3_state *s, float fps);
void w3_set_help(w3_state *s, int enabled);
void w3_set_crosshair(w3_state *s, int enabled);

#endif
