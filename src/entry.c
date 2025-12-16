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

#include "geometry/geometry_build.h"
#include "geometry/geometry_build.c"

#include "font/font_build.h"
#include "font/font_build.c"

#include "graphics/graphics_build.h"
#include "graphics/graphics_build.c"

#include "ubuntu_regular.c"
#include "font_awesome_7_solid.c"

var_global Arena   Permanent_Storage  = { };
var_global FO_Font Font               = { };

fn_internal void next_frame(B32 first_frame, Platform_Render_Context *render_context) {
  If_Unlikely(first_frame) {
    r_init(render_context);
    g2_init();

    Str ubuntu_font = str(Ubuntu_Regular_ttf_len, Ubuntu_Regular_ttf);
    fo_font_init(&Font, &Permanent_Storage, ubuntu_font, 64, v2_u16(512, 512), Codepoints_ASCII);
  }

  g2_draw_rect(v2f(0, 64), v2f(100000, 2), .color = v4f(.8f, .3f, .3f, 1));
  g2_draw_rect(v2f(64, 0), v2f(2, 100000), .color = v4f(.2f, .8f, .3f, 1));

  g2_draw_text(str_lit("The quick brown fox, jumps over the lazy dog... !"), &Font, v2f(64, 64));
  g2_draw_text(str_lit("The quick brown fox, jumps over the lazy dog... !"), &Font, v2f(64, 64), .rot_deg = 90);

  g2_frame_flush();
  r_frame_flush();
}

fn_internal void log_core_context(void) {
  Log_Zone_Scope("hardware info") {
    log_info("CPU: %.*s",            str_expand(core_context()->cpu_name));
    log_info("Logical Cores: %llu",  core_context()->cpu_logical_cores);
    log_info("Page Size: %$$llu",    core_context()->mmu_page_bytes);
    log_info("RAM Capacity: %$$llu", core_context()->ram_capacity_bytes);
  }
}

fn_internal void platform_entry_point(Array_Str command_line, Platform_Bootstrap *boot) {
  boot->next_frame = next_frame;
  boot->title = str_lit("Alice Engine");

  logger_push_hook(logger_write_entry_standard_stream, logger_format_entry_minimal);
  log_core_context();
} 

