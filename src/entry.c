// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

#include "core/core_build.h"
#include "core/core_build.c"

#include "base/base_build.h"
#include "base/base_build.c"
#include "base/base_test.c"

#include "platform/platform_build.h"
#include "platform/platform_build.c"

#include "render/render_build.h"
#include "render/render_build.c"

#include "graphics/graphics_build.h"
#include "graphics/graphics_build.c"

#include "font/font_build.h"
#include "font/font_build.c"

#include "geometry/geometry_build.h"
#include "geometry/geometry_build.c"

#include "ubuntu_regular.c"
#include "font_awesome_7_solid.c"

R_Texture Glyph_Texture;
V2_U16 atlas_size = { 512, 512 };

cb_function void next_frame(B32 first_frame, Platform_Render_Context *render_context) {
  If_Unlikely(first_frame) {
    r_init(render_context);
    g2_init();

    STBTT_backend_init();

    stbtt_fontinfo font = { };
    if (!stbtt_InitFont(&font, Ubuntu_Regular_ttf, 0)) {
      log_fatal("failed to load font");
    }

    I32 codepoint_start = ' ';
    I32 codepoint_end   = '~';

    Scratch scratch = { };
    Scratch_Scope(&scratch, 0) {
      U08 *texture_data = arena_push_count(scratch.arena, U08, 4 * atlas_size.x * atlas_size.y);
      F32 scale         = stbtt_ScaleForPixelHeight(&font, 64);
      
      Skyline_Packer sk = { };
      skyline_packer_init(&sk, scratch.arena, atlas_size);

      For_I64_Range(it_glyph, codepoint_start, codepoint_end) {
        I32 glyph_width  = 0;
        I32 glyph_height = 0;
        I32 glyph_x_off  = 0;
        I32 glyph_y_off  = 0;
        U08 *bitmap      = stbtt_GetCodepointBitmap(&font, 0, scale, it_glyph, &glyph_width, &glyph_height, &glyph_x_off, &glyph_y_off);

        V2_U16 packed_position = { };
        if (skyline_packer_push(&sk, v2_u16((U16)glyph_width, (U16)glyph_height), 1, &packed_position)) {
          For_U64(it_h, glyph_height) {
            For_U64(it_w, glyph_width) {
              I64 dst_it = ((packed_position.y + it_h) * atlas_size.y + (packed_position.x + it_w));
              I64 src_it = ((glyph_height - it_h - 1) * glyph_width + it_w);

              texture_data[4 * dst_it + 0] = bitmap[src_it];
              texture_data[4 * dst_it + 1] = bitmap[src_it];
              texture_data[4 * dst_it + 2] = bitmap[src_it];
              texture_data[4 * dst_it + 3] = bitmap[src_it];
            }
          }
        }
      }

      Glyph_Texture = r_texture_allocate(R_Texture_Format_RGBA_U08_Normalized, atlas_size.x, atlas_size.y);
      r_texture_download(Glyph_Texture, R_Texture_Format_RGBA_U08_Normalized, r2i(0, 0, atlas_size.x, atlas_size.y), texture_data);
    }

    STBTT_backend_free();
  }

  g2_draw_rect(v2f(0.f, 0.f), v2f(atlas_size.x, atlas_size.y), .tex = Glyph_Texture);

  g2_frame_flush();
  r_frame_flush();
}

cb_function void log_core_context(void) {
  Log_Zone_Scope("hardware info") {
    log_info("CPU: %.*s",            str_expand(core_context()->cpu_name));
    log_info("Logical Cores: %llu",  core_context()->cpu_logical_cores);
    log_info("Page Size: %$$llu",    core_context()->mmu_page_bytes);
    log_info("RAM Capacity: %$$llu", core_context()->ram_capacity_bytes);
  }
}

cb_function void platform_entry_point(Array_Str command_line, Platform_Bootstrap *boot) {
  boot->next_frame = next_frame;
  boot->title = str_lit("Alice Engine");

  logger_push_hook(logger_write_entry_standard_stream, logger_format_entry_minimal);
  log_core_context();
} 

