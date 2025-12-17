#ifndef SIMPLE3DO_H
#define SIMPLE3DO_H

#include <simple3d.h>

int shape_cube(s3d_vec3 center, float size, int r, int g, int b);
int shape_box(s3d_vec3 center, float w, float h, float d, int r, int g, int b);
int shape_pyramid(s3d_vec3 base, float size, float height, int r, int g, int b);
int shape_cone(s3d_vec3 base, float radius, float height, int segments, int r, int g, int b);
int shape_cylinder(s3d_vec3 base, float radius, float height, int segments, int r, int g, int b);
int shape_sphere_wire(s3d_vec3 center, float radius, int rings, int segments, int r, int g, int b);
int shape_axis(s3d_vec3 origin, float length);
int shape_grid_xz(s3d_vec3 center, int count, float spacing, int r, int g, int b);

int shape_cube_solid(s3d_vec3 center, float size, int r, int g, int b);
int shape_pyramid_solid(s3d_vec3 base, float size, float height, int r, int g, int b);

int shape_set_group(int first_obj, int group_id);
int shape_make_selectable(int first_obj, int group_id);

#endif
