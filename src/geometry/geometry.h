// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

// ------------------------------------------------------------
// #-- 3D Geometry Construction

typedef Array_V3F GEO3_Path;

typedef struct GEO3_Surface {
  Array_V3F vertices;
  Array_U32 indices;
} GEO3_Surface;

cb_function GEO3_Path     geo3_build_path(Arena *arena, GEO3_Path control_nodes, U64 oversample, U64 subdiv_level);
cb_function GEO3_Surface  geo3_build_tube(Arena *arena, GEO3_Path path, F32 radius, I32 resolution);

// ------------------------------------------------------------
// #-- File Export

// TODO(cmat): MOVE. FIX.
M4F m4f_look_at(V3F right, V3F up, V3F forward, V3F position) {
  return (M4F) {
    .e11 = right.x,
    .e21 = right.y,
    .e31 = right.z,
    .e41 = -v3f_dot(right, position),

    .e12 = up.x,
    .e22 = up.y,
    .e32 = up.z,
    .e42 = -v3f_dot(up, position),
    
    .e13 = forward.x,
    .e23 = forward.y,
    .e33 = forward.z,
    .e43 = -v3f_dot(forward, position),

    .e14 = 0.f,
    .e24 = 0.f,
    .e34 = 0.f,
    .e44 = 1.f,
  };
}

// NOTE(cmat): Homogenous projection matrix, z in [0, 1]
M4F m4f_projection(F32 fov_y_radians, F32 aspect_ratio, F32 near_z, F32 far_z) {
  F32 y_scale  = 1.f / tanf(.5f * fov_y_radians);
  F32 x_scale  = y_scale / aspect_ratio;
  F32 z_range  = far_z - near_z;
  F32 z_scale  = far_z / z_range;
  F32 wz_scale = -near_z * far_z / z_range;

  return (M4F) {
    .e11 = x_scale,
    .e22 = y_scale,
    .e33 = z_scale,
    .e34 = 1,
    .e43 = wz_scale,
  };
}

