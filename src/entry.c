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

#include "ui/ui_build.h"
#include "ui/ui_build.c"

#include "http/http_wasm.c"

#include "figtree_regular.c"
#include "font_awesome_7_solid.c"


#define ICON_FA_PLAY  "\xef\x81\x8b"	// U+f04b
#define ICON_FA_PAUSE "\xef\x81\x8c"	// U+f04c
#define ICON_FA_FILE  "\xef\x85\x9b"	// U+f15b

#define TEST_STR ICON_FA_PLAY " " ICON_FA_PAUSE " " ICON_FA_FILE

var_global FO_Font  UI_Font_Text       = { };
var_global FO_Font  UI_Font_Icon       = { };
var_global Arena    Permanent_Storage  = { };
var_global B32      camera_orthographic = 0;

var_global B32      context_menu       = 0;
var_global V2F      context_menu_at    = { };

Arena        request_arena  = { };
HTTP_Request request        = { };

R_Buffer    index_buffer;
R_Buffer    vertex_buffer;
R_Buffer    world_buffer;
R_Pipeline  pipeline;


typedef struct Camera {
  V3F look_at;
  F32 radius_m;
  F32 theta_deg;
  F32 phi_deg;
  F32 near_m;
  F32 far_m;
  F32 fov_deg;
  B32 orthographic;
} Camera;

var_global Camera camera = {
  .look_at      = { 0, 0, 0 },
  .radius_m     = 10.f,
  .theta_deg    = 0.f,
  .phi_deg      = 90.f,
  .near_m       = 1.f,
  .far_m        = 5000.f,
  .fov_deg      = 60.f,
  .orthographic = 0,
};

fn_internal M4F camera_view(Camera *camera) {
  F32 theta_rad = f32_radians_from_degrees(camera->theta_deg);
  F32 phi_rad   = f32_radians_from_degrees(camera->phi_deg);

  V3F position = v3f_mul(camera->radius_m, v3f(f32_cos(theta_rad) * f32_sin(phi_rad),
                                               f32_sin(phi_rad)   * f32_sin(phi_rad),
                                               f32_cos(phi_rad)));

  M4F view = m4f_hom_look_at(v3f(0, 1, 0), position, camera->look_at);
  return view;
}

fn_internal M4F camera_projection(Camera *camera) {
  F32 fov_rad = f32_radians_from_degrees(camera->fov_deg);

  M4F projection = { };
  if (!camera_orthographic) {
    projection = m4f_hom_perspective(platform_display()->aspect_ratio, fov_rad, camera->near_m, camera->far_m);
  } else {
    F32 h        = 1.f;
    F32 w        = h * platform_display()->aspect_ratio;

    V2F bottom_left = v2f(-.5f * w, -.5f * h);
    V2F top_right   = v2f(+.5f * w, +.5f * h);
    projection = m4f_hom_orthographic(bottom_left, top_right, camera->near_m, camera->far_m);
  }

  return projection;
}

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

    arena_init(&request_arena);
    http_request_send(&request, &request_arena, str_lit("stanford_bunny.obj"));


    F32 scale = 1000.0f;
    F32 vmin = -1.f * scale;
    F32 vmax = +1.f * scale;

    R_Vertex_XUC_3D test_vertices[] = {
      { .X = v3f(vmin, 0, vmin), .U = v2f(0.f, 0.f),      .C = abgr_u32_from_rgba_premul(v4f(1.f, 0.f, 0.f, 1.f)), },
      { .X = v3f(vmax, 0, vmin), .U = v2f(scale, 0.f),    .C = abgr_u32_from_rgba_premul(v4f(1.f, 1.f, 0.f, 1.f)), },
      { .X = v3f(vmax, 0, vmax), .U = v2f(scale, scale),  .C = abgr_u32_from_rgba_premul(v4f(1.f, 0.f, 1.f, 1.f)), },
      { .X = v3f(vmin, 0, vmax), .U = v2f(0.f, scale),    .C = abgr_u32_from_rgba_premul(v4f(1.f, 1.f, 1.f, 1.f)), },
    };

    U32 test_indices[] = { 0, 1, 2, 0, 2, 3 };

    pipeline = r_pipeline_create(R_Shader_Flat_3D, &R_Vertex_Format_XUC_3D);

    vertex_buffer = r_buffer_allocate(sizeof(test_vertices), R_Buffer_Mode_Static);
    r_buffer_download(vertex_buffer, 0, sizeof(test_vertices), test_vertices);

    index_buffer = r_buffer_allocate(sizeof(test_indices), R_Buffer_Mode_Static);
    r_buffer_download(index_buffer, 0, sizeof(test_indices), test_indices);
  }

  var_local_persist F32 timer = 0; timer += .5f * 1 / 200.f;

  M4F view = camera_view(&camera);
  M4F projection = camera_projection(&camera);
  M4F world_view_projection = m4f_mul(view, projection);

  R_Constant_Buffer_World_3D test_world = {
    .World_View_Projection = world_view_projection,
    .Eye_Position          = v3f(0, 0, 0),
  };

  world_buffer = r_buffer_allocate(sizeof(R_Constant_Buffer_World_3D), R_Buffer_Mode_Static);
  r_buffer_download(world_buffer, 0, sizeof(test_world), &test_world);
 

  ui_frame_begin();
 
  if (context_menu && platform_input()->mouse.left.down_first_frame) {
    context_menu = 0;
  }

  if (platform_input()->mouse.right.down_first_frame) {
    context_menu = 1;
    context_menu_at = v2f(platform_input()->mouse.position.x, platform_display()->resolution.y - platform_input()->mouse.position.y);
  }

  if (context_menu) {
    ui_set_next_overlay();
    UI_Node *context_container = ui_container(str_lit("context_menu"), UI_Container_Mode_Box, Axis2_Y, UI_Size_Fit, UI_Size_Fit);
    context_container->flags |= UI_Flag_Layout_Float_X;
    context_container->flags |= UI_Flag_Layout_Float_Y;

    context_container->flags |= UI_Flag_Draw_Clip_Content;
    context_container->flags |= UI_Flag_Draw_Shadow;

    context_container->flags |= UI_Flag_Animation_Fade_In;
    context_container->flags |= UI_Flag_Animation_Grow_X;
    context_container->flags |= UI_Flag_Animation_Grow_Y;

    context_container->layout.float_position[Axis2_X] = (I32)context_menu_at.x;
    context_container->layout.float_position[Axis2_Y] = (I32)context_menu_at.y;

    UI_Parent_Scope(context_container) {
      ui_button(str_lit("option 1"));
      ui_button(str_lit("option 2"));
      ui_button(str_lit("option 3"));
    }
  }

  UI_Parent_Scope(ui_container(str_lit("fill"), UI_Container_Mode_Box, Axis2_Y, UI_Size_Fit, UI_Size_Fit)) {
    ui_checkbox(str_lit("orthographic"), &camera_orthographic);

#if 0
    UI_Font_Scope(&UI_Font_Icon) {
      ui_button(str_lit(ICON_FA_PLAY));
    }

    var_local_persist B32 checked = 0;
    ui_checkbox(str_lit("Testing Checkboxes 1"), &checked);
    ui_button(str_lit("button_test_1"));

    UI_Parent_Scope(ui_container(str_lit("fit"), UI_Container_Mode_Box, Axis2_Y, UI_Size_Fit, UI_Size_Fit)) {
      ui_label    (str_lit("Container Label"));
      ui_button   (str_lit("Container Button"));
      ui_checkbox (str_lit("Container Test"), &checked);
    }

    var_local_persist F32 value = 0;
    ui_edit_f32(str_lit("Float Value"), &value, .1f);
#endif
  }

  g2_draw_rect(v2f(0, 0), platform_display()->resolution, .color = v4f(.2f, .2f, .3f, 1));
  g2_submit_draw();

  R_Command_Draw draw = {
    .constant_buffer  = world_buffer,
    .vertex_buffer    = vertex_buffer,
    .index_buffer     = index_buffer,
    .pipeline         = pipeline,
    .texture          = R_Texture_White,
    .sampler          = R_Sampler_Linear_Clamp,

    .draw_index_count = 6,
    .draw_index_offset = 0,

    .depth_test        = 0,
    .draw_region       = r2i(0, 0, platform_display()->resolution.x, platform_display()->resolution.y),
    .clip_region       = r2i(0, 0, platform_display()->resolution.x, platform_display()->resolution.y),
  };

  r_command_push_draw(&draw);
 
  ui_frame_end();
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

