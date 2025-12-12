// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

// ------------------------------------------------------------
// #-- 2D

// TODO(cmat): Cleanup
// TODO(cmat): Switch to a linked-list based implementation. We will NEVER have more than a 1k nodes,
// Only like 100 even in the worse case scenario, so it's pointless doing allocating an array this large.
// Array shifts seem also unnecessary.

// https://jvernay.fr/en/blog/skyline-2d-packer/implementation/

typedef struct Skyline_Packer {
  V2_U16       atlas_size;
  Array_V2_U16 nodes;
} Skyline_Packer;

fn_internal void skyline_packer_init(Skyline_Packer *sk, Arena *arena, V2_U16 atlas_size) {
  sk->atlas_size = atlas_size;

  array_reserve(arena, &sk->nodes, atlas_size.x);
  array_push(&sk->nodes, v2_u16(0, 0));
}

fn_internal void skyline_packer_reset(Skyline_Packer *sk) {
  array_clear(&sk->nodes);
  array_push(&sk->nodes, v2_u16(0, 0));
}

fn_internal B32 skyline_packer_push(Skyline_Packer *sk, V2_U16 rect, U16 border, V2_U16 *packed_position) {
  rect.x += border;
  rect.y += border;

  U16 best_fit_y = u16_limit_max;
  U16 best_fit_x = u16_limit_max;

  U16 best_fit_start_idx = u16_limit_max;
  U16 best_fit_end_idx   = u16_limit_max;

  For_U16(it, sk->nodes.len) {
    V2_U16 p = sk->nodes.dat[it];

    if (p.x == 0) p.x += border;
    if (p.y == 0) p.y += border;

    if (p.x + rect.x > sk->atlas_size.x) break;
    if (p.y >= best_fit_y)               continue;

    // NOTE(cmat): Iterate over shadowed region,
    // - find maximal height.
    U16 shadow_end_x = p.x + rect.x;
  
    U16 it_shadow = 0;
    for (it_shadow = it + 1; it_shadow < sk->nodes.len; it_shadow++) {
      if (sk->nodes.dat[it_shadow].x >= shadow_end_x) break;
      if (p.y < sk->nodes.dat[it_shadow].y) {
        p.y = sk->nodes.dat[it_shadow].y;
      }
    }

    if (p.y >= best_fit_y)                continue;
    if (p.y + rect.y > sk->atlas_size.y)  continue;

    best_fit_x = p.x;
    best_fit_y = p.y;

    best_fit_start_idx = it;
    best_fit_end_idx   = it_shadow;
  }

  B32 result = 0;
  if (best_fit_start_idx != u16_limit_max) {
    result = 1;

    V2_U16 new_top_left     = v2_u16(best_fit_x, best_fit_y + rect.y);
    V2_U16 new_bottom_right = v2_u16(best_fit_x + rect.x, sk->nodes.dat[best_fit_end_idx - 1].y);

    B32 insert_bottom_right = (best_fit_end_idx   < sk->nodes.len) ? 
                              (new_bottom_right.x < sk->nodes.dat[best_fit_end_idx].x) :
                              (new_bottom_right.x < sk->atlas_size.x);

    U16 remove_count = best_fit_end_idx - best_fit_start_idx;
    U16 insert_count = 1 + insert_bottom_right;

    if (insert_count > remove_count) {
      U16 idx   = sk->nodes.len - 1;
      U16 idx_2 = idx + (insert_count - remove_count);

      while (idx >= best_fit_end_idx) {
        sk->nodes.dat[idx_2--] = sk->nodes.dat[idx--];
      }

      sk->nodes.len += (insert_count - remove_count);

    } else if (insert_count < remove_count) {
      U16 erase_count = remove_count - insert_count;
      array_erase(&sk->nodes, best_fit_end_idx - erase_count, erase_count);
    }

    sk->nodes.dat[best_fit_start_idx] = new_top_left;
    if (insert_bottom_right) {
      sk->nodes.dat[best_fit_start_idx + 1] = new_bottom_right;
    }
  }


  *packed_position = v2_u16(best_fit_x, best_fit_y);
  return result;
}


// ------------------------------------------------------------
// #-- 3D

typedef Array_V3F GEO3_Path;

typedef struct GEO3_Surface {
  Array_V3F vertices;
  Array_U32 indices;
} GEO3_Surface;

fn_internal GEO3_Path     geo3_build_path(Arena *arena, GEO3_Path control_nodes, U64 oversample, U64 subdiv_level);
fn_internal GEO3_Surface  geo3_build_tube(Arena *arena, GEO3_Path path, F32 radius, I32 resolution);

