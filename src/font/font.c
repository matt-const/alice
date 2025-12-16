// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

// ------------------------------------------------------------
// #-- NOTE(cmat): Link core layer functionality to stb backend
var_global Arena STBTT_Arena = { };

var_global void STBTT_backend_init() {
  arena_init(&STBTT_Arena);
}

var_global void STBTT_backend_free() {
  arena_destroy(&STBTT_Arena);
}

fn_internal void *STBTT_malloc_ext(U64 bytes) {
  void *data = arena_push_size(&STBTT_Arena, bytes, .flags = 0);
  return data;
}

fn_internal void STBTT_free_ext(void *ptr) {
  // NOTE(cmat): We free memory at STBTT_backend_free all at once.
}

#define stbtt_uint8   U08
#define stbtt_uint16  U16
#define stbtt_uint32  U32

#define stbtt_int8    I08
#define stbtt_int16   I16
#define stbtt_int32   I32

#define STBTT_ifloor(x)  (I32) f32_floor((F32)(x))
#define STBTT_iceil(x)   (I32) f32_ceil ((F32)(x))
#define STBTT_sqrt(x)    f32_sqrt((F32)(x))
#define STBTT_cos(x)     f32_cos((F32)(x))
#define STBTT_acos(x)    f32_acos((F32)(x))
#define STBTT_fabs(x)    f32_fabs((F32)(x))
#define STBTT_pow(x, y)  f32_pow((F32)(x), (F32)(y))
#define STBTT_fmod(x, y) f32_fmod((F32)(x), (F32)(y))

#define STBTT_malloc(x, u) ((void)(u), STBTT_malloc_ext(x))
#define STBTT_free(x, u)   ((void)(u), STBTT_free_ext(x))

#define STBTT_assert(x)   Assert(x, "stb_truetype runtime failure")
#define STBTT_strlen(x)   cstring_len

#define STBTT_memcpy      memory_copy
#define STBTT_memset      memory_fill

#define STB_TRUETYPE_IMPLEMENTATION
#include "thirdparty/stb_truetype.h"

fn_internal void fo_font_init(FO_Font *font, Arena *arena, Str font_data, I32 font_size, V2_U16 atlas_size, Array_Codepoint codepoints) {
  zero_fill(font);
  font->glyph_bucket_count = 1024;
  font->glyph_bucket_array = arena_push_count(arena, FO_Glyph_List, font->glyph_bucket_count);

  STBTT_backend_init();

  stbtt_fontinfo font_info = { };
  stbtt_InitFont(&font_info, font_data.txt, 0);

  Scratch scratch = { };
  Scratch_Scope(&scratch, arena) {
    U08 *texture_data = arena_push_count(scratch.arena, U08, 4 * atlas_size.x * atlas_size.y);
    F32 scale         = stbtt_ScaleForPixelHeight(&font_info, font_size);

    Skyline_Packer sk = { };
    skyline_packer_init(&sk, scratch.arena, atlas_size);

    For_I64(it_codepoint, codepoints.len) {
      U32 codepoint = codepoints.dat[it_codepoint];

      FO_Glyph *glyph = fo_font_glyph_add(font, arena, codepoint);

      I32 glyph_width  = 0;
      I32 glyph_height = 0;
      I32 glyph_x_off  = 0;
      I32 glyph_y_off  = 0;
      U08 *bitmap      = stbtt_GetCodepointBitmap(&font_info, 0, scale, codepoint, &glyph_width, &glyph_height, &glyph_x_off, &glyph_y_off);


      V2_U16 packed_position = { };
      if (glyph_width && glyph_height) {
        if (skyline_packer_push(&sk, v2_u16((U16)glyph_width, (U16)glyph_height), 5, &packed_position)) {

          For_U64(it_h, glyph_height) {
            For_U64(it_w, glyph_width) {
              I64 dst_it = ((packed_position.y + it_h) * atlas_size.x + (packed_position.x + it_w));
              I64 src_it = ((glyph_height - it_h - 1) * glyph_width + it_w);

              texture_data[4 * dst_it + 0] = bitmap[src_it];
              texture_data[4 * dst_it + 1] = bitmap[src_it];
              texture_data[4 * dst_it + 2] = bitmap[src_it];
              texture_data[4 * dst_it + 3] = bitmap[src_it];
            }
          }

          glyph->bounds   = v2i(glyph_width, glyph_height);
          glyph->atlas_uv = r2f(packed_position.x / (F32)atlas_size.x,
                                packed_position.y / (F32)atlas_size.y,
                                (packed_position.x + glyph_width)  / (F32)atlas_size.x,
                                (packed_position.y + glyph_height) / (F32)atlas_size.y);
        }
      } else {
        glyph->no_texture = 1;
      }

      I32 left_side_bearing = 0;
      I32 advance = 0;
      stbtt_GetCodepointHMetrics(&font_info, codepoint, &advance, &left_side_bearing);

      glyph->pen_offset             = v2i(glyph_x_off, -(I32)glyph_y_off - glyph_height);
      glyph->pen_advance            = scale * advance;
    }

    font->glyph_atlas_size = atlas_size;
    font->glyph_atlas = r_texture_allocate(R_Texture_Format_RGBA_U08_Normalized, atlas_size.x, atlas_size.y);
    r_texture_download(font->glyph_atlas, R_Texture_Format_RGBA_U08_Normalized, r2i(0, 0, atlas_size.x, atlas_size.y), texture_data);
  }

  STBTT_backend_free();
}

fn_internal FO_Glyph *fo_font_glyph_add(FO_Font *font, Arena *arena, Codepoint codepoint) {
  U64 bucket_index    = codepoint % font->glyph_bucket_count;
  FO_Glyph_List *list = font->glyph_bucket_array + bucket_index;

  if (!list->first) {
    list->first = arena_push_type(arena, FO_Glyph);
    list->last  = list->first;
  } else {
    list->last->hash_next = arena_push_type(arena, FO_Glyph);
    list->last            = list->last->hash_next;
  }

  list->last->codepoint = codepoint;
  return list->last;
}

fn_internal FO_Glyph *fo_font_glyph_get(FO_Font *font, Codepoint codepoint) {
  U64 bucket_index    = codepoint % font->glyph_bucket_count;
  FO_Glyph_List *list = font->glyph_bucket_array + bucket_index;

  FO_Glyph *entry = list->first;
  while (entry) {
    if (entry->codepoint == codepoint) {
      break;
    }

    entry = entry->hash_next;
  }

  return entry;
}

