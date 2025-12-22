// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

#include "ubuntu_regular.c"
#include "font_awesome_7_solid.c"

#define ICON_FA_PLAY  "\xef\x81\x8b"	// U+f04b
#define ICON_FA_PAUSE "\xef\x81\x8c"	// U+f04c
#define ICON_FA_FILE  "\xef\x85\x9b"	// U+f15b

#define TEST_STR ICON_FA_PLAY " " ICON_FA_PAUSE " " ICON_FA_FILE

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

var_global Arena   Permanent_Storage  = { };

fn_internal void next_frame(B32 first_frame, Platform_Render_Context *render_context) {
  If_Unlikely(first_frame) {
    r_init(render_context);
    g2_init();
    ui_init();
  }

  UI_Node *root = ui_node_push(str_lit("##root"), UI_Flag_Draw_Background);
  root->layout.gap_child  = 2.0f;
  root->layout.gap_border = 20;
  root->layout.size[Axis2_X] = UI_Size_Fixed(platform_display()->resolution.x);
  root->layout.size[Axis2_Y] = UI_Size_Fixed(platform_display()->resolution.y);
  root->layout.direction = Axis2_Y;

  root->draw.hsv_background = v3f(0.6f, .8f, .6f);

  ui_parent_push(root); {

    UI_Node *fit = ui_node_push(str_lit("##fit"), UI_Flag_Draw_Background);
    fit->layout.gap_child     = 2.0f;
    fit->layout.gap_border    = 20;
    fit->layout.size[Axis2_X] = UI_Size_Fit;
    fit->layout.size[Axis2_Y] = UI_Size_Fixed(platform_input()->mouse.position.y);
    fit->draw.hsv_background  = v3f(0.1f, .9f, .8f);
    fit->layout.direction     = Axis2_Y;

    ui_parent_push(fit); {

      UI_Node *fit_1 = ui_node_push(str_lit("##fit_1"), UI_Flag_Draw_Background);
      fit_1->layout.gap_child     = 2.0f;
      fit_1->layout.gap_border    = 20;
      fit_1->layout.size[Axis2_X] = UI_Size_Fit;
      fit_1->layout.size[Axis2_Y] = UI_Size_Fit;
      fit_1->draw.hsv_background  = v3f(0.3f, .9f, .8f);

      ui_parent_push(fit_1); {

        ui_button(str_lit("1, 1"));
        ui_button(str_lit("1, 2"));
        ui_button(str_lit("1, 3"));
        ui_button(str_lit("1, 4"));
        ui_button(str_lit("1, 5"));
        ui_button(str_lit("1, 6"));
        ui_button(str_lit("1, 7"));

      } ui_parent_pop();

      UI_Node *fill_1 = ui_node_push(str_lit("##fill_1"), UI_Flag_Draw_Background);
      fill_1->layout.gap_child  = 2.0f;
      fill_1->layout.gap_border = 20;
      fill_1->layout.size[Axis2_X] = UI_Size_Fill;
      fill_1->layout.size[Axis2_Y] = UI_Size_Fill;
      fill_1->draw.hsv_background = v3f(0.9f, .5f, .8f);
      ui_parent_push(fill_1); { } ui_parent_pop();

      UI_Node *fit_2 = ui_node_push(str_lit("##fit_2"), UI_Flag_Draw_Background);
      fit_2->layout.gap_child  = 2.0f;
      fit_2->layout.gap_border = 20;
      fit_2->layout.size[Axis2_X] = UI_Size_Fit;
      fit_2->layout.size[Axis2_Y] = UI_Size_Fit;
      fit_2->draw.hsv_background = v3f(0.3f, .9f, .8f);

      ui_parent_push(fit_2); {

        ui_button(str_lit("2, 1"));
        ui_button(str_lit("2, 2"));
        ui_button(str_lit("2, 3"));

      } ui_parent_pop();


      UI_Node *fill_2 = ui_node_push(str_lit("##fill_2"), UI_Flag_Draw_Background);
      fill_2->layout.gap_child  = 2.0f;
      fill_2->layout.gap_border = 20;
      fill_2->layout.size[Axis2_X] = UI_Size_Fill;
      fill_2->layout.size[Axis2_Y] = UI_Size_Fill;
      fill_2->draw.hsv_background = v3f(0.3f, .5f, .8f);
      ui_parent_push(fill_2); { } ui_parent_pop();

      UI_Node *fit_3 = ui_node_push(str_lit("##fit_3"), UI_Flag_Draw_Background);
      fit_3->layout.gap_child  = 2.0f;
      fit_3->layout.gap_border = 20;
      fit_3->layout.size[Axis2_X] = UI_Size_Fit;
      fit_3->layout.size[Axis2_Y] = UI_Size_Fit;
      fit_3->draw.hsv_background = v3f(0.3f, .9f, .8f);

      ui_parent_push(fit_3); {

        ui_button(str_lit("3, 1"));
        ui_button(str_lit("3, 2"));
        ui_button(str_lit("3, 3"));
        ui_button(str_lit("3, 4"));
        ui_button(str_lit("3, 5"));
        ui_button(str_lit("3, 6"));
        ui_button(str_lit("3, 7"));
        ui_button(str_lit("3, 8"));

      } ui_parent_pop();


    } ui_parent_pop();
  } ui_parent_pop();

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

