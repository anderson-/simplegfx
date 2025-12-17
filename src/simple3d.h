#ifndef SIMPLE3D_H
#define SIMPLE3D_H

#include <simplegfx.h>

#define S3D_MAX_OBJECTS 1024
#define S3D_MAX_GROUPS 64
#define S3D_FOV_DEG 70.0f
#define S3D_NEAR_PLANE 0.1f
#define S3D_FAR_PLANE 200.0f

typedef struct {
  float x, y, z;
} s3d_vec3;

typedef enum {
  S3D_LINE = 0,
  S3D_CIRCLE,
  S3D_TEXT,
  S3D_POINT,
  S3D_TRIANGLE
} s3d_obj_type;

typedef enum {
  S3D_MODE_FPS = 0,
  S3D_MODE_ORBIT
} s3d_camera_mode;

typedef struct {
  s3d_vec3 a, b;
  int thickness;
} s3d_line_data;

typedef struct {
  s3d_vec3 pos;
  float radius;
  int filled;
} s3d_circle_data;

typedef struct {
  s3d_vec3 pos;
  char label[48];
  int size;
} s3d_text_data;

typedef struct {
  s3d_vec3 pos;
} s3d_point_data;

typedef struct {
  s3d_vec3 a, b, c;
} s3d_tri_data;

typedef struct {
  s3d_obj_type type;
  int r, g, b;
  int group_id;
  int selectable;
  int visible;
  union {
    s3d_line_data line;
    s3d_circle_data circle;
    s3d_text_data text;
    s3d_point_data point;
    s3d_tri_data tri;
  } data;
} s3d_object;

typedef struct {
  int id;
  int active;
  s3d_vec3 offset;
  float rot_x, rot_y, rot_z;
} s3d_group;

typedef struct {
  s3d_vec3 pos;
  float yaw, pitch;
  float distance;
  s3d_vec3 target;
  float move_speed;
  float slow_speed;
  float look_speed;
  float zoom_speed;
  s3d_camera_mode mode;
} s3d_camera;

typedef struct {
  int forward, backward;
  int left, right;
  int up, down;
  int yaw_left, yaw_right;
  int pitch_up, pitch_down;
  int zoom_in, zoom_out;
  int slow;
} s3d_input;

typedef struct {
  s3d_object objects[S3D_MAX_OBJECTS];
  int obj_count;
  s3d_group groups[S3D_MAX_GROUPS];
  int group_count;
  
  int selected;
  int selected_group;
  int label_counter;
  
  s3d_camera cam;
  s3d_input input;
  
  uint32_t last_ticks;
  int menu_combo;
  
  int show_grid;
  int show_origin;
  int show_crosshair;
  int show_labels;
  int debug_pick;
  uint32_t debug_pick_until;
  
  int mouse_x, mouse_y;
  int hover_obj;
  int pick_result;
  
  int select_r, select_g, select_b;
  int hover_r, hover_g, hover_b;
  
  Uint32 *pick_buffer;
} s3d_state;

s3d_state* s3d_get_state(void);
void s3d_init(void);
void s3d_update(float dt);
void s3d_draw(void);

void s3d_clear_objs(void);
int s3d_add_line(s3d_vec3 p1, s3d_vec3 p2, int r, int g, int b);
int s3d_add_line_ex(s3d_vec3 p1, s3d_vec3 p2, int r, int g, int b, int thickness);
int s3d_add_circle(s3d_vec3 pos, float radius, int r, int g, int b, int filled);
int s3d_add_text(s3d_vec3 pos, const char *label, int r, int g, int b, int size);
int s3d_add_point(s3d_vec3 pos, int r, int g, int b);
int s3d_add_tri(s3d_vec3 a, s3d_vec3 b, s3d_vec3 c, int r, int g, int bl);
void s3d_remove_obj(int idx);

void s3d_set_selectable(int idx, int selectable);
void s3d_set_visible(int idx, int visible);
void s3d_set_group(int idx, int group_id);
void s3d_set_color(int idx, int r, int g, int b);

int s3d_new_group(void);
void s3d_move_group(int group_id, s3d_vec3 offset);
void s3d_rotate_group(int group_id, float rx, float ry, float rz);
void s3d_delete_group(int group_id);

void s3d_select_obj(int idx);
void s3d_select_next(void);
void s3d_select_prev(void);
int s3d_get_selected(void);
int s3d_get_hover(void);

void s3d_set_mode(s3d_camera_mode mode);
s3d_camera_mode s3d_get_mode(void);
void s3d_toggle_mode(void);

void s3d_set_cam_pos(s3d_vec3 pos);
void s3d_set_cam_target(s3d_vec3 target);
s3d_vec3 s3d_get_cam_pos(void);
s3d_vec3 s3d_forward(void);

void s3d_show_grid(int show);
void s3d_show_origin(int show);
void s3d_show_crosshair(int show);
void s3d_show_labels(int show);
void s3d_debug_pick(int enable);
s3d_vec3 s3d_placement_pos(float fixed_dist);

void s3d_set_mouse(int x, int y);
int s3d_pick_at(int x, int y);
int s3d_do_pick(void);
void s3d_draw_pick(void);

int s3d_handle_key(char key, int down);

s3d_vec3 s3d_vec(float x, float y, float z);
s3d_vec3 s3d_add(s3d_vec3 a, s3d_vec3 b);
s3d_vec3 s3d_sub(s3d_vec3 a, s3d_vec3 b);
s3d_vec3 s3d_scale(s3d_vec3 v, float s);
float s3d_len(s3d_vec3 v);
s3d_vec3 s3d_norm(s3d_vec3 v);
s3d_vec3 s3d_cross(s3d_vec3 a, s3d_vec3 b);
float s3d_dot(s3d_vec3 a, s3d_vec3 b);

#endif
