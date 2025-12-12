// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

fn_internal PBD_Mass_Points pbd_reserve_mass_points(Arena *arena, U32 point_count) {
  PBD_Mass_Points result = { };
  zero_fill(&result);

  result.count           = point_count;
  result.positions       = arena_push_count(arena, V3F, point_count);
  result.positions_last  = arena_push_count(arena, V3F, point_count);
  result.velocities      = arena_push_count(arena, V3F, point_count);
  result.masses_inverse  = arena_push_count(arena, F32, point_count);

  return result;
}

fn_internal PBD_Springs pbd_reserve_springs(Arena *arena, U32 spring_count) {
  PBD_Springs result = { };
  zero_fill(&result);

  result.count        = spring_count;
  result.joints       = arena_push_count(arena, V2U, spring_count);
  result.rest_lengths = arena_push_count(arena, F32, spring_count);

  return result;
}

fn_internal void pbd_step(PBD_Mass_Points *points, PBD_Springs *springs, F32 dt, U32 substeps) {

  // NOTE(cmat): Update positions.
  For_U32(it, points->count) {
    points->positions_last[it] = points->positions[it];
    points->positions[it]      = v3f_add(points->positions[it], v3f_mul(dt, points->velocities[it]));
  }

  For_U32(it_substep, substeps) {

    // NOTE(cmat): Correct positions based on constraints.
    For_U32(it, springs->count) {
      V2U joint     = springs->joints[it];
      F32 rest_len  = springs->rest_lengths[it];
      
      V3F x1        = points->positions[joint.x];
      V3F x2        = points->positions[joint.y];
   
      V3F diff      = v3f_sub(x2, x1);
      F32 len       = v3f_len(diff);
      V3F normal    = v3f_noz(diff);

      F32 w1        = points->masses_inverse[joint.x];
      F32 w2        = points->masses_inverse[joint.y];
      F32 w12       = w1 + w2;

      F32 c1        = +1.f * (f32_div_safe(w1, w12) * (len - rest_len));
      F32 c2        = -1.f * (f32_div_safe(w2, w12) * (len - rest_len));

      points->positions[joint.x] = v3f_add(points->positions[joint.x], v3f_mul(c1, normal));
      points->positions[joint.y] = v3f_add(points->positions[joint.y], v3f_mul(c2, normal));
    }
  }

  // NOTE(cmat): Compute velocities.
  For_U32(it, points->count) {
    V3F step_diff           = v3f_sub(points->positions[it], points->positions_last[it]);
    points->velocities[it]  = v3f_div(step_diff, dt);
  }

  For_U32(it, points->count) {
    points->velocities[it]  = v3f_mul(1.f - 0.1f, points->velocities[it]);
  }
}

