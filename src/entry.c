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


#define ICON_FA_PLAY  "\xef\x81\x8b"	// U+f04b
#define ICON_FA_PAUSE "\xef\x81\x8c"	// U+f04c
#define ICON_FA_FILE  "\xef\x85\x9b"	// U+f15b

#define TEST_STR ICON_FA_PLAY " " ICON_FA_PAUSE " " ICON_FA_FILE

var_global Arena   Permanent_Storage  = { };
var_global FO_Font UI_Font            = { };
var_global FO_Font ICO_Font           = { };

fn_internal void next_frame(B32 first_frame, Platform_Render_Context *render_context) {
  If_Unlikely(first_frame) {
    r_init(render_context);
    g2_init();

    Str ubuntu_font = str(Ubuntu_Regular_ttf_len, Ubuntu_Regular_ttf);
    fo_font_init(&UI_Font, &Permanent_Storage, ubuntu_font, 50, v2_u16(512, 512), Codepoints_ASCII);

    Codepoint codepoints_ico[] = {
      codepoint_from_utf8(str_lit(ICON_FA_PLAY), 0),
      codepoint_from_utf8(str_lit(ICON_FA_PAUSE), 0),
      codepoint_from_utf8(str_lit(ICON_FA_FILE), 0),
      ' ',
    };

    Str font_awesome_font = str(Font_Awesome_7_Free_Solid_900_otf_len, Font_Awesome_7_Free_Solid_900_otf);
    fo_font_init(&ICO_Font, &Permanent_Storage, font_awesome_font, 50, v2_u16(512, 512), array_from_sarray(Array_Codepoint, codepoints_ico));
  }

  g2_draw_rect(v2f(-5000, platform_input()->mouse.position.y), v2f(10000, 2));
  g2_draw_rect(v2f(platform_input()->mouse.position.x, -5000), v2f(2, 10000));

  g2_draw_text(str_lit("AVAV The quick brown fox, jumps over the lazy dog... !"), &UI_Font, platform_input()->mouse.position);
  g2_draw_text(str_lit("AVAV The quick brown fox, jumps over the lazy dog... !"), &UI_Font, platform_input()->mouse.position, .rot_deg = 90);

  g2_draw_text(str_lit(TEST_STR), &ICO_Font, v2f_add(platform_input()->mouse.position, v2f(50, 50)));

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

