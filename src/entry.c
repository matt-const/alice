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

#include "ui/ui.h"
#include "ui/ui.c"

#include "figtree_regular.c"
#include "font_awesome_7_solid.c"

#define ICON_FA_PLAY  "\xef\x81\x8b"	// U+f04b
#define ICON_FA_PAUSE "\xef\x81\x8c"	// U+f04c
#define ICON_FA_FILE  "\xef\x85\x9b"	// U+f15b

#define TEST_STR ICON_FA_PLAY " " ICON_FA_PAUSE " " ICON_FA_FILE

var_global FO_Font UI_Font_Text     = { };
var_global FO_Font UI_Font_Icon     = { };
var_global Arena Permanent_Storage  = { };
var_global B32 render_stuff         = 1;

fn_internal void next_frame(B32 first_frame, Platform_Render_Context *render_context) {
  If_Unlikely(first_frame) {
    r_init(render_context);
    g2_init();

    Codepoint icon_codepoints[] = {
      codepoint_from_utf8(str_lit(ICON_FA_FILE), 0),
      codepoint_from_utf8(str_lit(ICON_FA_PAUSE), 0),
      codepoint_from_utf8(str_lit(ICON_FA_PLAY), 0),
    };

    fo_font_init(&UI_Font_Text, &Permanent_Storage,
                 str(figtree_regular_ttf_len, figtree_regular_ttf),
                 26, v2_u16(1024, 1024), Codepoints_ASCII);

    fo_font_init(&UI_Font_Icon, &Permanent_Storage,
                 str(Font_Awesome_7_Free_Solid_900_otf_len, Font_Awesome_7_Free_Solid_900_otf),
                 26, v2_u16(1024, 1024), array_from_sarray(Array_Codepoint, icon_codepoints));

    ui_init(&UI_Font_Text);
  }

  UI_Node *root = ui_node_push(str_lit("##root"), 0);
  root->layout.gap_child      = 2.0f;
  root->layout.gap_border[Axis2_X] = 5;
  root->layout.gap_border[Axis2_Y] = 5;
  root->layout.size[Axis2_X]  = UI_Size_Fixed(platform_display()->resolution.x);
  root->layout.size[Axis2_Y]  = UI_Size_Fixed(platform_display()->resolution.y);
  root->layout.direction      = Axis2_Y;

  var_local_persist HSV color = { };

  UI_Parent_Scope(root) {
    if (ui_button(str_lit("Button Test")).press) {
      render_stuff = !render_stuff;
    }

    UI_Font_Scope(&UI_Font_Icon) {
      ui_button(str_lit(ICON_FA_PLAY));
    }

    var_local_persist B32 checked = 0;
    ui_checkbox(str_lit("Checkbox Test"), &checked);
    // ui_button(str_lit("button_test_1"));
  
    UI_Parent_Scope(ui_container(str_lit("fit"), UI_Container_Mode_Box, Axis2_Y, UI_Size_Fit, UI_Size_Fit)) {
      ui_label(str_lit("Container Label"));
      ui_button(str_lit("Container Button"));

    var_local_persist B32 checked = 0;
      ui_checkbox(str_lit("Container Checkbox"), &checked);
    }

    var_local_persist F32 value = 0;
    ui_edit_f32(str_lit("Float Value"), &value, .1f);
  }

#if 0

  UI_Parent_Scope(root) {

    UI_Parent_Scope(ui_container(str_lit("fit"), Axis2_Y, UI_Size_Fit, UI_Size_Fixed(700))) {
     
      UI_Parent_Scope(ui_container(str_lit("fit_1"), Axis2_X, UI_Size_Fit, UI_Size_Fit)) {
        ui_button(str_lit("Button 1"));
        ui_button(str_lit("Button 2"));
      }

      UI_Parent_Scope(ui_container(str_lit("fit_2"), Axis2_X, UI_Size_Fit, UI_Size_Fit)) {
        ui_button(str_lit("Button 1"));
        ui_button(str_lit("Button 2"));
        ui_button(str_lit("Button 3"));
      }

      UI_Parent_Scope(ui_container(str_lit("fill"), Axis2_X, UI_Size_Fill, UI_Size_Fill));

      UI_Parent_Scope(ui_container(str_lit("fit_3"), Axis2_X, UI_Size_Fit, UI_Size_Fit)) {
        if (ui_button(str_lit("Draw Background")).click) {
          render_stuff = !render_stuff;
        }
      }
    }
  }
#endif

  if (render_stuff) {
    g2_draw_rect(v2f(0, 0), platform_display()->resolution, .color = v4f(.2f, .2f, .6f, 1));
  }

  ui_frame_flush(root);

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

