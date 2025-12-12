// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

typedef struct PBD_Mass_Points {
  U32  count;
  V3F *positions;
  V3F *positions_last;
  V3F *velocities;
  F32 *masses_inverse;
} PBD_Mass_Points;

typedef struct PBD_Springs {
  U32  count;
  V2U *joints;
  F32 *rest_lengths;
} PBD_Springs;

fn_internal PBD_Mass_Points pbd_reserve_mass_points   (Arena *arena, U32 point_count);
fn_internal PBD_Springs     pbd_reserve_springs       (Arena *arena, U32 spring_count);
fn_internal void            pbd_step                  (PBD_Mass_Points *points, PBD_Springs *springs, F32 dt, U32 substeps);


