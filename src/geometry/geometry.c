// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

// ------------------------------------------------------------
// #-- 3D Geometry Construction

cb_function GEO3_Path geo3_build_path(Arena *arena, GEO3_Path control_nodes, U64 oversample, U64 subdiv_level) {

  GEO3_Path result = {};
  Scratch_Scope(scratch, arena) {

    Array_F32 node_weights = {};
    array_reserve(scratch.arena, &node_weights, oversample + 1);

    Array_U32 node_subdivs = {};
    array_reserve(scratch.arena, &node_subdivs, control_nodes.len * oversample);

    U64 node_subdiv_count = 0;
    For_U64(it_control, control_nodes.len - 1) {
      V3F p1 = (it_control == 0) ? control_nodes.dat[0] : control_nodes.dat[it_control - 1];
      V3F p2 = control_nodes.dat[it_control];
      V3F p3 = control_nodes.dat[it_control + 1];
      V3F p4 = (it_control + 2 < control_nodes.len) ? control_nodes.dat[it_control + 2] :  control_nodes.dat[control_nodes.len - 1];

      // NOTE(cmat): Subdivide curve based on the weight differences between
      // control nodes.
      F32 curvature_sum = 0.f;
      V3F last_tangent = v3f_spline_catmull_dt(.0f, p1, p2, p3, p4);
      array_push(&node_weights, 0);

      For_U64 (it, oversample) {
        F32 t          = (F32)(it + 1) / (F32)oversample;
        V3F tangent    = v3f_spline_catmull_dt(t, p1, p2, p3, p4);
        curvature_sum += f32_abs(acosf(v3f_dot(v3f_noz(last_tangent), v3f_noz(tangent))));

        array_push(&node_weights, curvature_sum);
        last_tangent = tangent;
      }

      For_U64(it, oversample) {
        F32 curvature_dt    = node_weights.dat[it + 1] - node_weights.dat[it];
        F32 curvature_ratio = curvature_dt / curvature_sum;
        U32 subdiv          = (U32)(curvature_ratio * (F32)subdiv_level) + 1;
        subdiv              = u32_min(subdiv, subdiv_level);

        array_push(&node_subdivs, subdiv);
        node_subdiv_count += subdiv;
      }

      array_clear(&node_weights);
    }

    // NOTE(cmat): Allocate result, perform subdivision.
    array_reserve(arena, &result, node_subdiv_count);

    U64 node_subdiv_at = 0;
    For_U64(it_control, control_nodes.len - 1) {
      V3F p1 = (it_control == 0) ? control_nodes.dat[0] : control_nodes.dat[it_control - 1];
      V3F p2 = control_nodes.dat[it_control];
      V3F p3 = control_nodes.dat[it_control + 1];
      V3F p4 = (it_control + 2 < control_nodes.len) ? control_nodes.dat[it_control + 2] :  control_nodes.dat[control_nodes.len - 1];

      For_U64(it, oversample) {
        U32 step_count = node_subdivs.dat[node_subdiv_at++];
        For_U32(step, step_count) {
          F32 t = ((F32)step / (F32)step_count + (F32)it) / (F32)oversample;
          array_push(&result, v3f_spline_catmull(t, p1, p2, p3, p4));
        }
      }
    }
  }

  return result;
}

cb_function GEO3_Surface geo3_build_tube (Arena *arena, GEO3_Path path, F32 radius, I32 resolution) {
  Assert(path.len > 1, "need at least 2 path nodes to construct a tube");

  U64 vertex_count  = resolution * path.len;
  U64 index_count   = 3 * 2 * resolution * (path.len - 1);

  GEO3_Surface surface = { };
  array_reserve(arena, &surface.vertices, vertex_count);
  array_reserve(arena, &surface.indices,  index_count);

  V3F last_up = v3f(0.f, 1.f, 0.f);

  // NOTE(cmat): Vertices
  For_U64(it, path.len) {
    V3F p       = path.dat[it];
    V3F forward = v3f(0.f, 0.f, 0.f);

    if (it + 1 < path.len) {
      forward = v3f_sub(path.dat[it + 1], p);
    } else {
      forward = v3f_sub(p, path.dat[it - 1]);
    }

    forward = v3f_noz(forward);

    V3F right = v3f_noz(v3f_cross(last_up, forward));
    if (v3f_len(right) < NOZ_Epsilon) {
      right = v3f(1.f, 0, 0);
    }

    V3F up  = v3f_noz(v3f_cross(forward, right));
    last_up = up;

    For_U64(it_disk, resolution) {
      F32 theta = (F32)it_disk / (F32)resolution * f32_2pi;
      F32 ct    = f32_cos(theta);
      F32 st    = f32_sin(theta);

      V3F dir        = v3f_add(v3f_mul(ct, right), v3f_mul(st, up));
      V3F disk_point = v3f_add(p, v3f_mul(radius, dir));

      array_push(&surface.vertices, disk_point);
    }
  }

  // NOTE(cmat): Indices.
  For_U32(it_node, path.len - 1) {
    For_U32(it_disk, resolution) {
      U32 base_index = resolution * it_node;
      U32 top_index  = resolution * (it_node + 1);

      U32 base_1 = base_index + it_disk;
      U32 base_2 = base_index + ((it_disk + 1) % resolution);

      U32 top_1  = top_index + it_disk;
      U32 top_2  = top_index + ((it_disk + 1) % resolution);

      array_push(&surface.indices, base_1);
      array_push(&surface.indices, base_2);
      array_push(&surface.indices, top_2);

      array_push(&surface.indices, base_1);
      array_push(&surface.indices, top_2);
      array_push(&surface.indices, top_1);
    }
  }

  return surface;
}

