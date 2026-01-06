// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

// ------------------------------------------------------------
// #-- Graphics 2D Immediate Mode

#define G2_Vertex_Array_Capacity u64_millions(1)
#define G2_Index_Array_Capactiy  u64_millions(2)

typedef struct G2_Buffer {
  U64               vertex_at;
  U64               vertex_capacity;
  R_Vertex_XUC_2D  *vertex_array;

  U64               index_at;
  U64               index_capacity;
  U32              *index_array;

  U32               draw_index_base;
  U32               draw_index_count;
} G2_Buffer;

typedef U32 G2_Draw_Mode;
enum {
  G2_Draw_Mode_Flat,

  G2_Draw_Mode_Count,
};

var_global struct {
  Arena               arena;
  G2_Buffer           buffer;
  R_Buffer            vertex_buffer;
  R_Buffer            index_buffer;
  R_Buffer            constant_viewport_2D;
  R_Texture           texture;
  R_Pipeline          pipelines[G2_Draw_Mode_Count];
  G2_Draw_Mode        draw_mode;

  R2I                 last_clip_region;
  R2I                 active_clip_region;
} G2_State;

fn_internal void g2_init(void) {
  arena_init(&G2_State.arena);
  
  G2_State.buffer.vertex_capacity = G2_Vertex_Array_Capacity;
  G2_State.buffer.index_capacity  = G2_Index_Array_Capactiy;

  G2_State.buffer.vertex_array = arena_push_count(&G2_State.arena, R_Vertex_XUC_2D, G2_State.buffer.vertex_capacity );
  G2_State.buffer.index_array  = arena_push_count(&G2_State.arena, U32,             G2_State.buffer.index_capacity  );

  G2_State.vertex_buffer = r_buffer_allocate(G2_State.buffer.vertex_capacity * sizeof(R_Vertex_XUC_2D), R_Buffer_Mode_Dynamic);
  G2_State.index_buffer  = r_buffer_allocate(G2_State.buffer.index_capacity  * sizeof(U32),             R_Buffer_Mode_Dynamic);

  G2_State.pipelines[G2_Draw_Mode_Flat] = r_pipeline_create(R_Shader_Flat_2D, &R_Vertex_Format_XUC_2D, 0);
  // G2_State.pipelines[G2_Draw_Mode_MTSDF]  = r_pipeline_create(R_Shader_MTSDF_2D,  &R_Vertex_Format_XUC_2D);

  G2_State.texture              = R_Texture_White;
  G2_State.constant_viewport_2D = r_buffer_allocate(sizeof(R_Constant_Buffer_Viewport_2D), R_Buffer_Mode_Dynamic);
  G2_State.last_clip_region     = r2i(0, 0, i32_limit_max, i32_limit_max);
  G2_State.active_clip_region   = G2_State.last_clip_region;
}

fn_internal void g2_submit_draw(void) {
  if (G2_State.buffer.draw_index_count) {

    // TODO(cmat): Move out some parts.
    V2F display_size = pl_display()->resolution;
    r_buffer_download(G2_State.constant_viewport_2D,
                        0, sizeof(R_Constant_Buffer_Viewport_2D),
                        &(R_Constant_Buffer_Viewport_2D) {
                          .NDC_From_Screen = {
                          .e11 = 2.f / display_size.width,
                          .e22 = 2.f / display_size.height,
                          .e33 = 0,
                          .e44 = 1,
                          .e14 = -1,
                          .e24 = -1,
                          .e34 =  0 }
                        });

    V2F resolution = pl_display()->resolution;

    R2I clip_region = G2_State.last_clip_region;

    clip_region.x0 = i32_max(0, clip_region.x0);
    clip_region.y0 = i32_max(0, clip_region.y0);
    clip_region.x1 = i32_min((I32)pl_display()->resolution.x, clip_region.x1);
    clip_region.y1 = i32_min((I32)pl_display()->resolution.y, clip_region.y1);

    R_Command_Draw draw = {
        .constant_buffer       = G2_State.constant_viewport_2D,
        .vertex_buffer         = G2_State.vertex_buffer,
        .index_buffer          = G2_State.index_buffer,
        .pipeline              = G2_State.pipelines[G2_State.draw_mode],
        .texture               = G2_State.texture,
        .sampler               = R_Sampler_Linear_Clamp,
        
        .draw_index_count      = G2_State.buffer.draw_index_count,
        .draw_index_offset     = G2_State.buffer.index_at - G2_State.buffer.draw_index_count,

        .depth_test            = 0,

        .draw_region           = pl_display_region(),
        .clip_region           = clip_region,
    };

    r_command_push_draw(&draw);
    G2_State.buffer.draw_index_count  = 0;
    G2_State.last_clip_region         = G2_State.active_clip_region;
  }
}

fn_internal void g2_frame_flush(void) {
  g2_submit_draw();
  if (G2_State.buffer.index_at && G2_State.buffer.vertex_at) {
    r_buffer_download(G2_State.vertex_buffer,  0, G2_State.buffer.vertex_at * sizeof(R_Vertex_XUC_2D),  (U08 *)G2_State.buffer.vertex_array);
    r_buffer_download(G2_State.index_buffer,   0, G2_State.buffer.index_at *  sizeof(U32),              (U08 *)G2_State.buffer.index_array);
  }
  
  G2_State.buffer.vertex_at         = 0;
  G2_State.buffer.index_at          = 0;
  G2_State.buffer.draw_index_base   = 0;
  G2_State.buffer.draw_index_count  = 0;
  G2_State.draw_mode                = 0;

  G2_State.last_clip_region   = r2i(0, 0, i32_limit_max, i32_limit_max);
  G2_State.active_clip_region = G2_State.last_clip_region;
}

typedef struct {
  R_Vertex_XUC_2D *vertices;
  U32             *indices;
  U32              base_index;
} G2_Draw_Entry;

fn_internal G2_Draw_Entry g2_push_draw(U32 vertex_count, U32 index_count, R_Texture texture, G2_Draw_Mode mode) {
  Assert(G2_State.buffer.vertex_at  + vertex_count  < G2_State.buffer.vertex_capacity,  "2D immediate vertex buffer overflow");
  Assert(G2_State.buffer.index_at   + index_count   < G2_State.buffer.index_capacity,   "2D immediate index buffer overflow");

  // NOTE(cmat): If we need to change the draw mode, or we exceed the number of textures
  // - we can bind, we flush with a draw call.
  B32 submit_draw = 0;
  if (G2_State.buffer.draw_index_count && G2_State.draw_mode != mode) {
    submit_draw = 1;
  }

  if (G2_State.last_clip_region.x0 != G2_State.active_clip_region.x0 ||
      G2_State.last_clip_region.y0 != G2_State.active_clip_region.y0 ||
      G2_State.last_clip_region.x1 != G2_State.active_clip_region.x1 ||
      G2_State.last_clip_region.y1 != G2_State.active_clip_region.y1) {

    submit_draw = 1;
  }

  if (texture != G2_State.texture) {
    submit_draw = 1;
  }

  if (submit_draw) {
    g2_submit_draw();
  }
  
  G2_Draw_Entry entry = {
    .base_index   = G2_State.buffer.draw_index_base,
    .vertices     = G2_State.buffer.vertex_array + G2_State.buffer.vertex_at,
    .indices      = G2_State.buffer.index_array  + G2_State.buffer.index_at,
  };

  G2_State.buffer.draw_index_base   += vertex_count;
  G2_State.buffer.draw_index_count  += index_count;
  G2_State.buffer.vertex_at         += vertex_count;
  G2_State.buffer.index_at          += index_count;
  G2_State.draw_mode                 = mode;
  G2_State.texture                   = texture;

  return entry;
}

fn_internal void g2_draw_tri_ext(G2_Tri *tri) {
  G2_Draw_Entry entry = g2_push_draw(3, 3, tri->tex, G2_Draw_Mode_Flat);
  entry.indices[0]  = entry.base_index;
  entry.indices[1]  = entry.base_index + 1;
  entry.indices[2]  = entry.base_index + 2;
  
  U32 packed_color  = abgr_u32_from_rgba_premul(tri->color);
  entry.vertices[0] = (R_Vertex_XUC_2D) { .X = tri->x1, .C = packed_color, .U = tri->u1, };
  entry.vertices[1] = (R_Vertex_XUC_2D) { .X = tri->x2, .C = packed_color, .U = tri->u2, };
  entry.vertices[2] = (R_Vertex_XUC_2D) { .X = tri->x3, .C = packed_color, .U = tri->u3, };
}

fn_internal void g2_draw_rect_grad_ext(G2_Rect_Grad *rect) {
  G2_Draw_Entry entry = g2_push_draw(4, 6, rect->tex, G2_Draw_Mode_Flat);

  entry.indices[0] = entry.base_index;
  entry.indices[1] = entry.base_index + 1;
  entry.indices[2] = entry.base_index + 2;
  entry.indices[3] = entry.base_index;
  entry.indices[4] = entry.base_index + 2;
  entry.indices[5] = entry.base_index + 3;
  
  U32 packed_color_0 = abgr_u32_from_rgba_premul(rect->color_array[0]);
  U32 packed_color_1 = abgr_u32_from_rgba_premul(rect->color_array[1]);
  U32 packed_color_2 = abgr_u32_from_rgba_premul(rect->color_array[2]);
  U32 packed_color_3 = abgr_u32_from_rgba_premul(rect->color_array[3]);

  V2F X0 = rect->pos;
  V2F X1 = v2f_add(rect->pos, v2f(rect->size.x, 0));
  V2F X2 = v2f_add(rect->pos, rect->size);
  V2F X3 = v2f_add(rect->pos, v2f(0, rect->size.y));

  V2F U0 = rect->uv_array[0];
  V2F U1 = rect->uv_array[1];
  V2F U2 = rect->uv_array[2];
  V2F U3 = rect->uv_array[3];
  
  entry.vertices[0] = (R_Vertex_XUC_2D) { .X = X0, .C = packed_color_0, .U = U0, };
  entry.vertices[1] = (R_Vertex_XUC_2D) { .X = X1, .C = packed_color_1, .U = U1, };
  entry.vertices[2] = (R_Vertex_XUC_2D) { .X = X2, .C = packed_color_2, .U = U2, };
  entry.vertices[3] = (R_Vertex_XUC_2D) { .X = X3, .C = packed_color_3, .U = U3, };
}

fn_internal void g2_draw_rect_ext(G2_Rect *rect) {
  G2_Draw_Entry entry = g2_push_draw(4, 6, rect->tex, G2_Draw_Mode_Flat);

  entry.indices[0] = entry.base_index;
  entry.indices[1] = entry.base_index + 1;
  entry.indices[2] = entry.base_index + 2;
  entry.indices[3] = entry.base_index;
  entry.indices[4] = entry.base_index + 2;
  entry.indices[5] = entry.base_index + 3;
  
  U32 packed_color = abgr_u32_from_rgba_premul(rect->color);

  V2F X0 = rect->pos;
  V2F X1 = v2f_add(rect->pos, v2f(rect->size.x, 0));
  V2F X2 = v2f_add(rect->pos, rect->size);
  V2F X3 = v2f_add(rect->pos, v2f(0, rect->size.y));

  V2F U0 = rect->uv_bl;
  V2F U1 = v2f(rect->uv_tr.x, rect->uv_bl.y);
  V2F U2 = rect->uv_tr;
  V2F U3 = v2f(rect->uv_bl.x, rect->uv_tr.y);
  
  entry.vertices[0] = (R_Vertex_XUC_2D) { .X = X0, .C = packed_color, .U = U0, };
  entry.vertices[1] = (R_Vertex_XUC_2D) { .X = X1, .C = packed_color, .U = U1, };
  entry.vertices[2] = (R_Vertex_XUC_2D) { .X = X2, .C = packed_color, .U = U2, };
  entry.vertices[3] = (R_Vertex_XUC_2D) { .X = X3, .C = packed_color, .U = U3, };
}

fn_internal void g2_draw_rect_rounded_ext(G2_Rect_Rounded *rect) {
  U32 vertex_count = 3 * 4 + rect->segments * 4;

  U32 index_count = 0;
  if (rect->segments) {
    index_count = 3 * 6 + 4 * (6 + (rect->segments - 1) * 3);
  } else {
    index_count = 3 * 6 + 4 * 6;
  }

  G2_Draw_Entry entry = g2_push_draw(vertex_count, index_count, R_Texture_White, G2_Draw_Mode_Flat);

  U32 index_at = 0;

  // NOTE(cmat): Inner + Outer rectangles.
  For_U32 (it, 3) {
    entry.indices[index_at++] = entry.base_index + 4 * it + 0;
    entry.indices[index_at++] = entry.base_index + 4 * it + 1;
    entry.indices[index_at++] = entry.base_index + 4 * it + 2;

    entry.indices[index_at++] = entry.base_index + 4 * it + 0;
    entry.indices[index_at++] = entry.base_index + 4 * it + 2;
    entry.indices[index_at++] = entry.base_index + 4 * it + 3;
  }

  // NOTE(cmat): TL corner.
#define Push_Corner_Indices(corner_index_, first_, center_, last_)                      \
do {                                                                                    \
  U32 offset = 12 + corner_index_ * rect->segments;                                     \
  if (rect->segments) {                                                                 \
    entry.indices[index_at++] = entry.base_index + first_;                              \
    entry.indices[index_at++] = entry.base_index + center_;                             \
    entry.indices[index_at++] = entry.base_index + offset + 0;                          \
    For_U32(it, rect->segments - 1) {                                                   \
      entry.indices[index_at++] = entry.base_index + offset + it;                       \
      entry.indices[index_at++] = entry.base_index + center_;                           \
      entry.indices[index_at++] = entry.base_index + offset + it + 1;                   \
    }                                                                                   \
    entry.indices[index_at++] = entry.base_index + offset + (rect->segments - 1);       \
    entry.indices[index_at++] = entry.base_index + center_;                             \
    entry.indices[index_at++] = entry.base_index + last_;                               \
  } else {                                                                              \
    entry.indices[index_at++] = entry.base_index + first_;                              \
    entry.indices[index_at++] = entry.base_index + center_;                             \
    entry.indices[index_at++] = entry.base_index + last_;                               \
  }                                                                                     \
} while (0)

  Push_Corner_Indices(0, 3, 4,  7);
  Push_Corner_Indices(1, 6, 5,  2);
  Push_Corner_Indices(2, 8, 11, 0);
  Push_Corner_Indices(3, 1, 10, 9);

  U32 vertex_at = 0;
  U32 packed_color = abgr_u32_from_rgba_premul(rect->color);

  // NOTE(cmat): Inner vertices.
  V2F inner_bl = v2f(rect->pos.x, rect->pos.y + rect->radius);
  V2F inner_tr = v2f(rect->pos.x + rect->size.x, rect->pos.y + rect->size.y - rect->radius);

  entry.vertices[vertex_at++] = (R_Vertex_XUC_2D) { .X = inner_bl,                    .C = packed_color, .U = v2f(0.f, 0.f) };
  entry.vertices[vertex_at++] = (R_Vertex_XUC_2D) { .X = v2f(inner_tr.x, inner_bl.y), .C = packed_color, .U = v2f(1.f, 0.f) };
  entry.vertices[vertex_at++] = (R_Vertex_XUC_2D) { .X = inner_tr,                    .C = packed_color, .U = v2f(1.f, 1.f) };
  entry.vertices[vertex_at++] = (R_Vertex_XUC_2D) { .X = v2f(inner_bl.x, inner_tr.y), .C = packed_color, .U = v2f(0.f, 1.f) };
 
  // NOTE(cmat): Upper vertices
  V2F upper_bl = v2f(rect->pos.x + rect->radius, inner_tr.y);
  V2F upper_tr = v2f(rect->pos.x + rect->size.x - rect->radius, rect->pos.y + rect->size.y);

  entry.vertices[vertex_at++] = (R_Vertex_XUC_2D) { .X = upper_bl,                    .C = packed_color, .U = v2f(0.f, 0.f) };
  entry.vertices[vertex_at++] = (R_Vertex_XUC_2D) { .X = v2f(upper_tr.x, upper_bl.y), .C = packed_color, .U = v2f(1.f, 0.f) };
  entry.vertices[vertex_at++] = (R_Vertex_XUC_2D) { .X = upper_tr,                    .C = packed_color, .U = v2f(1.f, 1.f) };
  entry.vertices[vertex_at++] = (R_Vertex_XUC_2D) { .X = v2f(upper_bl.x, upper_tr.y), .C = packed_color, .U = v2f(0.f, 1.f) };

  // NOTE(cmat): Lower vertices
  V2F lower_bl = v2f(rect->pos.x + rect->radius,                rect->pos.y);
  V2F lower_tr = v2f(rect->pos.x + rect->size.x - rect->radius, rect->pos.y + rect->radius);

  entry.vertices[vertex_at++] = (R_Vertex_XUC_2D) { .X = lower_bl,                    .C = packed_color, .U = v2f(0.f, 0.f) };
  entry.vertices[vertex_at++] = (R_Vertex_XUC_2D) { .X = v2f(lower_tr.x, lower_bl.y), .C = packed_color, .U = v2f(1.f, 0.f) };
  entry.vertices[vertex_at++] = (R_Vertex_XUC_2D) { .X = lower_tr,                    .C = packed_color, .U = v2f(1.f, 1.f) };
  entry.vertices[vertex_at++] = (R_Vertex_XUC_2D) { .X = v2f(lower_bl.x, lower_tr.y), .C = packed_color, .U = v2f(0.f, 1.f) };

  // NOTE(cmat): Segments TL
  F32 corner_theta_table[]  = { 2 * f32_hpi, f32_hpi, 3 * f32_hpi, 0 };
  V2F corner_pos_table[]    = {
    v2f(inner_bl.x + rect->radius, inner_tr.y),
    v2f(inner_tr.x - rect->radius, inner_tr.y),
    v2f(inner_bl.x + rect->radius, inner_bl.y),
    v2f(inner_tr.x - rect->radius, inner_bl.y) };

  For_U32 (it_corner, 4) {
    F32 corner_theta = corner_theta_table[it_corner];
    V2F corner_pos   = corner_pos_table[it_corner];
    For_U32 (it_seg, rect->segments) {
      F32 theta = corner_theta - f32_hpi * ((F32)(it_seg + 1) / (F32)(rect->segments + 1));
      V2F offset = v2f_mul(rect->radius, v2f(f32_cos(theta), f32_sin(theta)));
      V2F pos = v2f_add(corner_pos, offset);
      entry.vertices[vertex_at++] = (R_Vertex_XUC_2D) { .X = pos, .C = packed_color, .U = v2f(0.f, 0.f) };
    }
  }

  Assert(vertex_at == vertex_count, "vertex_at != vertex_count");
  Assert(index_at  == index_count,  "index_at  != index_count");
}

fn_internal void g2_draw_disk_ext(G2_Disk *disk) {
  U32 resolution = disk->resolution;
  G2_Draw_Entry entry = g2_push_draw(resolution + 1, 3 * resolution, R_Texture_White, G2_Draw_Mode_Flat);

  For_U32(it, resolution) {
    entry.indices[3 * it + 0] = entry.base_index;
    entry.indices[3 * it + 1] = entry.base_index + 1 + it;
    entry.indices[3 * it + 2] = entry.base_index + 1 + ((it + 1) % resolution);
  }

  U32 packed_color = abgr_u32_from_rgba_premul(disk->color);
  entry.vertices[0] = (R_Vertex_XUC_2D) { .X = disk->pos, .C = packed_color, .U = v2f(0, 0), };
  For_U32(it, resolution) {
    F32 theta = (F32)it / (F32)resolution * f32_2pi;

    V2F position = v2f_add(disk->pos, v2f_mul(disk->radius, v2f(f32_cos(theta), f32_sin(theta))));
    entry.vertices[it + 1] = (R_Vertex_XUC_2D) {
      .X            = position,
      .C            = packed_color,
      .U            = v2f(0, 0),
    };
  }
}

fn_internal void g2_draw_line_ext(G2_Line *line) {
  G2_Draw_Entry entry = g2_push_draw(4, 6, R_Texture_White, G2_Draw_Mode_Flat);

  entry.indices[0] = entry.base_index + 0;
  entry.indices[1] = entry.base_index + 1;
  entry.indices[2] = entry.base_index + 2;

  entry.indices[3] = entry.base_index + 0;
  entry.indices[4] = entry.base_index + 2;
  entry.indices[5] = entry.base_index + 3;

  V2F delta    = v2f_noz(v2f_sub(line->end, line->start));
  V2F normal_1 = v2f_mul(line->thickness / 2, v2f(-delta.y,  delta.x));
  V2F normal_2 = v2f_mul(line->thickness / 2, v2f( delta.y, -delta.x));

  U32 packed_color = abgr_u32_from_rgba_premul(line->color);
  entry.vertices[0] = (R_Vertex_XUC_2D) { .X = v2f_add(line->start, normal_1), .C = packed_color, .U = v2f(0, 0), };
  entry.vertices[1] = (R_Vertex_XUC_2D) { .X = v2f_add(line->start, normal_2), .C = packed_color, .U = v2f(0, 0), };
  entry.vertices[2] = (R_Vertex_XUC_2D) { .X = v2f_add(line->end,   normal_2), .C = packed_color, .U = v2f(0, 0), };
  entry.vertices[3] = (R_Vertex_XUC_2D) { .X = v2f_add(line->end,   normal_1), .C = packed_color, .U = v2f(0, 0), };
}

fn_internal void g2_draw_text_ext(G2_Text *text) {
  U32 draw_glyph_count = 0;

  I32 decode_at = 0;
  for (;;) {
    if (decode_at >= text->text.len) break;
    Str decode_str = str_slice(text->text, decode_at, text->text.len - decode_at);

    I32 advance = 0;
    U32 codepoint = codepoint_from_utf8(decode_str, &advance);
    if (!advance) {
      advance = 1;
    }

    decode_at += advance;

    FO_Glyph *g = fo_glyph_get(text->font, codepoint);
    draw_glyph_count += !g->no_texture;
  }

  G2_Draw_Entry entry = g2_push_draw(4 * draw_glyph_count, 6 * draw_glyph_count, text->font->glyph_atlas, G2_Draw_Mode_Flat);
  U32 index_at = 0;
  U32 vertex_at = 0;
  
  For_U32(it, draw_glyph_count) {
    U32 base_index = entry.base_index + 4 * it;
    entry.indices[index_at++] = base_index + 0;
    entry.indices[index_at++] = base_index + 1;
    entry.indices[index_at++] = base_index + 2;
    entry.indices[index_at++] = base_index + 0;
    entry.indices[index_at++] = base_index + 2;
    entry.indices[index_at++] = base_index + 3;
  }

  U32 packed_color = abgr_u32_from_rgba_premul(text->color);

  V2F draw_at = text->pos;
  
  decode_at = 0;
  for (;;) {
    if (decode_at >= text->text.len) break;
    Str decode_str = str_slice(text->text, decode_at, text->text.len - decode_at);

    I32 advance = 0;
    U32 codepoint = codepoint_from_utf8(decode_str, &advance);
    if (!advance) {
      advance = 1;
    }

    decode_at += advance;

    FO_Glyph *g = fo_glyph_get(text->font, codepoint);
    if (!g->no_texture) {

      V2F offset = v2f(g->pen_offset.x, g->pen_offset.y);

      V2F x_array[] = {
        v2f_add(draw_at, offset),
        v2f_add(draw_at, v2f(offset.x + g->bounds.x, offset.y)),
        v2f_add(draw_at, v2f(offset.x + g->bounds.x, offset.y + g->bounds.y)),
        v2f_add(draw_at, v2f(offset.x, offset.y + g->bounds.y)),
      };

      if (text->rot_deg > 0.f) {
        F32 angle_rad = f32_radians_from_degrees(text->rot_deg);
        F32 c = f32_cos(angle_rad);
        F32 s = f32_sin(angle_rad);
        
        For_U32(it, sarray_len(x_array)) {
          V2F x = v2f_sub(x_array[it], text->pos);
          x = v2f(c * x.x - s * x.y, s * x.x + c * x.y);
          x_array[it] = v2f_add(x, text->pos);
        }
      }

      V2F U0 = g->atlas_uv.min;
      V2F U1 = v2f(g->atlas_uv.max.x, g->atlas_uv.min.y);
      V2F U2 = g->atlas_uv.max;
      V2F U3 = v2f(g->atlas_uv.min.x, g->atlas_uv.max.y);

      entry.vertices[vertex_at++] = (R_Vertex_XUC_2D) { .X = x_array[0], .C = packed_color, .U = U0, };
      entry.vertices[vertex_at++] = (R_Vertex_XUC_2D) { .X = x_array[1], .C = packed_color, .U = U1, };
      entry.vertices[vertex_at++] = (R_Vertex_XUC_2D) { .X = x_array[2], .C = packed_color, .U = U2, };
      entry.vertices[vertex_at++] = (R_Vertex_XUC_2D) { .X = x_array[3], .C = packed_color, .U = U3, };
    }

    draw_at.x += g->pen_advance;
  }
}

fn_internal void g2_clip_region(R2I region) {
  G2_State.active_clip_region = region;
}
