// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

// ------------------------------------------------------------
// #-- 2D

cb_function B32 geo2_pack_rect(GEO2_Pack_Rect *r_pack, V2I rect, );

// ------------------------------------------------------------
// #-- 3D

typedef Array_V3F GEO3_Path;

typedef struct GEO3_Surface {
  Array_V3F vertices;
  Array_U32 indices;
} GEO3_Surface;

cb_function GEO3_Path     geo3_build_path(Arena *arena, GEO3_Path control_nodes, U64 oversample, U64 subdiv_level);
cb_function GEO3_Surface  geo3_build_tube(Arena *arena, GEO3_Path path, F32 radius, I32 resolution);

