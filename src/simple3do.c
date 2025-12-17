#include <simple3do.h>
#include <math.h>

int shape_cube(s3d_vec3 c, float size, int r, int g, int b) {
  s3d_state *s = s3d_get_state();
  if (!s) return -1;
  float h = size * 0.5f;
  s3d_vec3 v[8] = {
    s3d_vec(c.x - h, c.y - h, c.z - h),
    s3d_vec(c.x + h, c.y - h, c.z - h),
    s3d_vec(c.x + h, c.y + h, c.z - h),
    s3d_vec(c.x - h, c.y + h, c.z - h),
    s3d_vec(c.x - h, c.y - h, c.z + h),
    s3d_vec(c.x + h, c.y - h, c.z + h),
    s3d_vec(c.x + h, c.y + h, c.z + h),
    s3d_vec(c.x - h, c.y + h, c.z + h)
  };
  int first = s->obj_count;
  s3d_add_line(v[0], v[1], r, g, b);
  s3d_add_line(v[1], v[2], r, g, b);
  s3d_add_line(v[2], v[3], r, g, b);
  s3d_add_line(v[3], v[0], r, g, b);
  s3d_add_line(v[4], v[5], r, g, b);
  s3d_add_line(v[5], v[6], r, g, b);
  s3d_add_line(v[6], v[7], r, g, b);
  s3d_add_line(v[7], v[4], r, g, b);
  s3d_add_line(v[0], v[4], r, g, b);
  s3d_add_line(v[1], v[5], r, g, b);
  s3d_add_line(v[2], v[6], r, g, b);
  s3d_add_line(v[3], v[7], r, g, b);
  return first;
}

int shape_box(s3d_vec3 c, float w, float h, float d, int r, int g, int b) {
  s3d_state *st = s3d_get_state();
  if (!st) return -1;
  float hw = w * 0.5f, hh = h * 0.5f, hd = d * 0.5f;
  s3d_vec3 v[8] = {
    s3d_vec(c.x - hw, c.y - hh, c.z - hd),
    s3d_vec(c.x + hw, c.y - hh, c.z - hd),
    s3d_vec(c.x + hw, c.y + hh, c.z - hd),
    s3d_vec(c.x - hw, c.y + hh, c.z - hd),
    s3d_vec(c.x - hw, c.y - hh, c.z + hd),
    s3d_vec(c.x + hw, c.y - hh, c.z + hd),
    s3d_vec(c.x + hw, c.y + hh, c.z + hd),
    s3d_vec(c.x - hw, c.y + hh, c.z + hd)
  };
  int first = st->obj_count;
  s3d_add_line(v[0], v[1], r, g, b);
  s3d_add_line(v[1], v[2], r, g, b);
  s3d_add_line(v[2], v[3], r, g, b);
  s3d_add_line(v[3], v[0], r, g, b);
  s3d_add_line(v[4], v[5], r, g, b);
  s3d_add_line(v[5], v[6], r, g, b);
  s3d_add_line(v[6], v[7], r, g, b);
  s3d_add_line(v[7], v[4], r, g, b);
  s3d_add_line(v[0], v[4], r, g, b);
  s3d_add_line(v[1], v[5], r, g, b);
  s3d_add_line(v[2], v[6], r, g, b);
  s3d_add_line(v[3], v[7], r, g, b);
  return first;
}

int shape_pyramid(s3d_vec3 base, float size, float height, int r, int g, int b) {
  s3d_state *st = s3d_get_state();
  if (!st) return -1;
  float h = size * 0.5f;
  s3d_vec3 v[5] = {
    s3d_vec(base.x - h, base.y, base.z - h),
    s3d_vec(base.x + h, base.y, base.z - h),
    s3d_vec(base.x + h, base.y, base.z + h),
    s3d_vec(base.x - h, base.y, base.z + h),
    s3d_vec(base.x, base.y + height, base.z)
  };
  int first = st->obj_count;
  s3d_add_line(v[0], v[1], r, g, b);
  s3d_add_line(v[1], v[2], r, g, b);
  s3d_add_line(v[2], v[3], r, g, b);
  s3d_add_line(v[3], v[0], r, g, b);
  s3d_add_line(v[0], v[4], r, g, b);
  s3d_add_line(v[1], v[4], r, g, b);
  s3d_add_line(v[2], v[4], r, g, b);
  s3d_add_line(v[3], v[4], r, g, b);
  return first;
}

int shape_cone(s3d_vec3 base, float radius, float height, int segments, int r, int g, int b) {
  s3d_state *st = s3d_get_state();
  if (!st) return -1;
  if (segments < 3) segments = 3;
  if (segments > 32) segments = 32;
  
  int first = st->obj_count;
  s3d_vec3 tip = s3d_vec(base.x, base.y + height, base.z);
  s3d_vec3 pts[32];
  
  for (int i = 0; i < segments; i++) {
    float angle = (float)i / (float)segments * 2.0f * M_PI;
    pts[i] = s3d_vec(base.x + cosf(angle) * radius, base.y, base.z + sinf(angle) * radius);
  }
  
  for (int i = 0; i < segments; i++) {
    s3d_add_line(pts[i], pts[(i + 1) % segments], r, g, b);
    s3d_add_line(pts[i], tip, r, g, b);
  }
  
  return first;
}

int shape_cylinder(s3d_vec3 base, float radius, float height, int segments, int r, int g, int b) {
  s3d_state *st = s3d_get_state();
  if (!st) return -1;
  if (segments < 3) segments = 3;
  if (segments > 32) segments = 32;
  
  int first = st->obj_count;
  s3d_vec3 bot[32], top[32];
  
  for (int i = 0; i < segments; i++) {
    float angle = (float)i / (float)segments * 2.0f * M_PI;
    float cx = cosf(angle) * radius;
    float cz = sinf(angle) * radius;
    bot[i] = s3d_vec(base.x + cx, base.y, base.z + cz);
    top[i] = s3d_vec(base.x + cx, base.y + height, base.z + cz);
  }
  
  for (int i = 0; i < segments; i++) {
    s3d_add_line(bot[i], bot[(i + 1) % segments], r, g, b);
    s3d_add_line(top[i], top[(i + 1) % segments], r, g, b);
    s3d_add_line(bot[i], top[i], r, g, b);
  }
  
  return first;
}

int shape_sphere_wire(s3d_vec3 center, float radius, int rings, int segments, int r, int g, int b) {
  s3d_state *st = s3d_get_state();
  if (!st) return -1;
  if (rings < 2) rings = 2;
  if (segments < 4) segments = 4;
  if (rings > 16) rings = 16;
  if (segments > 32) segments = 32;
  
  int first = st->obj_count;
  
  for (int i = 0; i <= rings; i++) {
    float phi = (float)i / (float)rings * M_PI;
    float y = center.y + cosf(phi) * radius;
    float ring_r = sinf(phi) * radius;
    
    s3d_vec3 prev;
    for (int j = 0; j <= segments; j++) {
      float theta = (float)j / (float)segments * 2.0f * M_PI;
      s3d_vec3 pt = s3d_vec(
        center.x + cosf(theta) * ring_r,
        y,
        center.z + sinf(theta) * ring_r
      );
      if (j > 0) {
        s3d_add_line(prev, pt, r, g, b);
      }
      prev = pt;
    }
  }
  
  for (int j = 0; j < segments; j++) {
    float theta = (float)j / (float)segments * 2.0f * M_PI;
    s3d_vec3 prev;
    for (int i = 0; i <= rings; i++) {
      float phi = (float)i / (float)rings * M_PI;
      float y = center.y + cosf(phi) * radius;
      float ring_r = sinf(phi) * radius;
      s3d_vec3 pt = s3d_vec(
        center.x + cosf(theta) * ring_r,
        y,
        center.z + sinf(theta) * ring_r
      );
      if (i > 0) {
        s3d_add_line(prev, pt, r, g, b);
      }
      prev = pt;
    }
  }
  
  return first;
}

int shape_axis(s3d_vec3 origin, float length) {
  s3d_state *st = s3d_get_state();
  if (!st) return -1;
  int first = st->obj_count;
  int x = s3d_add_line(origin, s3d_vec(origin.x + length, origin.y, origin.z), 255, 80, 80);
  int y = s3d_add_line(origin, s3d_vec(origin.x, origin.y + length, origin.z), 80, 255, 80);
  int z = s3d_add_line(origin, s3d_vec(origin.x, origin.y, origin.z + length), 80, 80, 255);
  s3d_set_selectable(x, 0);
  s3d_set_selectable(y, 0);
  s3d_set_selectable(z, 0);
  return first;
}

int shape_grid_xz(s3d_vec3 center, int count, float spacing, int r, int g, int b) {
  s3d_state *st = s3d_get_state();
  if (!st) return -1;
  int first = st->obj_count;
  float half = (float)count * spacing * 0.5f;
  
  for (int i = 0; i <= count; i++) {
    float pos = -half + (float)i * spacing;
    int idx1 = s3d_add_line(
      s3d_vec(center.x - half, center.y, center.z + pos),
      s3d_vec(center.x + half, center.y, center.z + pos), r, g, b);
    int idx2 = s3d_add_line(
      s3d_vec(center.x + pos, center.y, center.z - half),
      s3d_vec(center.x + pos, center.y, center.z + half), r, g, b);
    s3d_set_selectable(idx1, 0);
    s3d_set_selectable(idx2, 0);
  }
  
  return first;
}

int shape_set_group(int first_obj, int group_id) {
  s3d_state *st = s3d_get_state();
  if (!st) return 0;
  int count = 0;
  for (int i = first_obj; i < st->obj_count; i++) {
    s3d_set_group(i, group_id);
    s3d_set_selectable(i, 0);
    count++;
  }
  return count;
}

int shape_make_selectable(int first_obj, int group_id) {
  s3d_state *st = s3d_get_state();
  if (!st) return 0;
  int count = 0;
  for (int i = first_obj; i < st->obj_count; i++) {
    s3d_set_group(i, group_id);
    s3d_set_selectable(i, 0);
    count++;
  }
  if (first_obj < st->obj_count) {
    s3d_set_selectable(first_obj, 1);
  }
  return count;
}

int shape_cube_solid(s3d_vec3 c, float size, int r, int g, int b) {
  s3d_state *st = s3d_get_state();
  if (!st) return -1;
  float h = size * 0.5f;
  s3d_vec3 v[8] = {
    s3d_vec(c.x - h, c.y - h, c.z - h),
    s3d_vec(c.x + h, c.y - h, c.z - h),
    s3d_vec(c.x + h, c.y + h, c.z - h),
    s3d_vec(c.x - h, c.y + h, c.z - h),
    s3d_vec(c.x - h, c.y - h, c.z + h),
    s3d_vec(c.x + h, c.y - h, c.z + h),
    s3d_vec(c.x + h, c.y + h, c.z + h),
    s3d_vec(c.x - h, c.y + h, c.z + h)
  };
  int first = st->obj_count;
  s3d_add_tri(v[0], v[1], v[2], r, g, b); s3d_add_tri(v[0], v[2], v[3], r, g, b);
  s3d_add_tri(v[4], v[6], v[5], r, g, b); s3d_add_tri(v[4], v[7], v[6], r, g, b);
  s3d_add_tri(v[0], v[4], v[5], r, g, b); s3d_add_tri(v[0], v[5], v[1], r, g, b);
  s3d_add_tri(v[2], v[6], v[7], r, g, b); s3d_add_tri(v[2], v[7], v[3], r, g, b);
  s3d_add_tri(v[0], v[3], v[7], r, g, b); s3d_add_tri(v[0], v[7], v[4], r, g, b);
  s3d_add_tri(v[1], v[5], v[6], r, g, b); s3d_add_tri(v[1], v[6], v[2], r, g, b);
  return first;
}

int shape_pyramid_solid(s3d_vec3 base, float size, float height, int r, int g, int b) {
  s3d_state *st = s3d_get_state();
  if (!st) return -1;
  float h = size * 0.5f;
  s3d_vec3 v[5] = {
    s3d_vec(base.x - h, base.y, base.z - h),
    s3d_vec(base.x + h, base.y, base.z - h),
    s3d_vec(base.x + h, base.y, base.z + h),
    s3d_vec(base.x - h, base.y, base.z + h),
    s3d_vec(base.x, base.y + height, base.z)
  };
  int first = st->obj_count;
  s3d_add_tri(v[0], v[2], v[1], r, g, b); s3d_add_tri(v[0], v[3], v[2], r, g, b);
  s3d_add_tri(v[0], v[1], v[4], r, g, b);
  s3d_add_tri(v[1], v[2], v[4], r, g, b);
  s3d_add_tri(v[2], v[3], v[4], r, g, b);
  s3d_add_tri(v[3], v[0], v[4], r, g, b);
  return first;
}
