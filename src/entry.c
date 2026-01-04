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

#include "stl.h"

#include "figtree_regular.c"
#include "font_awesome_7_solid.c"

U32 fps_at = 0;
F32 fps_ring[200] = { };

#define ICON_FA_PLAY  "\xef\x81\x8b"	// U+f04b
#define ICON_FA_PAUSE "\xef\x81\x8c"	// U+f04c
#define ICON_FA_FORWARD "\xef\x81\x8e"	// U+f04e
#define ICON_FA_FORWARD_FAST "\xef\x81\x90"	// U+f050
#define ICON_FA_FORWARD_STEP "\xef\x81\x91"	// U+f051
#define ICON_FA_FONT "\xef\x80\xb1"	// U+f031
#define ICON_FA_EYE "\xef\x81\xae"	// U+f06e

#define ICON_FA_FILE  "\xef\x85\x9b"	// U+f15b
#define ICON_FA_CUBE "\xef\x86\xb2"	// U+f1b2

#define TEST_STR ICON_FA_PLAY " " ICON_FA_PAUSE " " ICON_FA_FILE

var_global FO_Font  UI_Font_Text       = { };
var_global FO_Font  UI_Font_Icon       = { };
var_global Arena    Permanent_Storage  = { };

var_global B32      context_menu       = 0;
var_global V2F      context_menu_at    = { };

Arena        request_arena  = { };
HTTP_Request request        = { };

R_Buffer    index_buffer;
R_Buffer    vertex_buffer;
R_Buffer    world_buffer;
R_Pipeline  pipeline;

B32         loaded_model;
R_Buffer    model_index_buffer;
R_Buffer    model_vertex_buffer;
U32         model_index_count;
R_Pipeline  model_pipeline;

typedef struct Camera {
  V3F look_at;
  F32 radius_m;
  F32 theta_deg;
  F32 phi_deg;

  F32 radius_m_t;
  F32 theta_deg_t;
  F32 phi_deg_t;

  F32 near_m;
  F32 far_m;
  F32 fov_deg;

  B32 orthographic;
  F32 orthographic_t;

  F32 aspect_ratio;
} Camera;

var_global Camera camera = {
  .look_at      = { 0, 0, 0 },
  .radius_m     = 10.f,
  .theta_deg    = 45.f,
  .phi_deg      = 45.f,
  .near_m       = 1.f,
  .far_m        = 5000.f,
  .fov_deg      = 60.f,
  .orthographic = 0,
};

fn_internal M4F camera_view(Camera *camera) {
  F32 theta_rad = f32_radians_from_degrees(camera->theta_deg_t);
  F32 phi_rad   = f32_radians_from_degrees(camera->phi_deg_t);

  V3F position = v3f_mul(camera->radius_m_t, v3f(f32_cos(theta_rad) * f32_sin(phi_rad),
                                                 f32_cos(phi_rad),
                                                 f32_sin(theta_rad) * f32_sin(phi_rad)));

  M4F view = m4f_hom_look_at(v3f(0, 1, 0), position, camera->look_at);
  return view;
}

fn_internal void camera_update(Camera *camera, R2F draw_region) {
  F32 frame_delta = platform_display()->frame_delta;
  
  camera->aspect_ratio = (draw_region.x1 - draw_region.x0) / (draw_region.y1 - draw_region.y0);

  camera->radius_m = f32_clamp(camera->radius_m, 3.f, 50.f);
  camera->phi_deg  = f32_clamp(camera->phi_deg,  0.01f, 179.99f);

  camera->radius_m_t      = f32_exp_smoothing(camera->radius_m_t,     camera->radius_m,     frame_delta * 15.f);
  camera->theta_deg_t     = f32_exp_smoothing(camera->theta_deg_t,    camera->theta_deg,    frame_delta * 15.f);
  camera->phi_deg_t       = f32_exp_smoothing(camera->phi_deg_t,      camera->phi_deg,      frame_delta * 15.f);

  camera->orthographic_t  = f32_exp_smoothing(camera->orthographic_t, camera->orthographic, frame_delta * 15.f);
}

fn_internal M4F camera_projection(Camera *camera) {
  F32 fov_rad = f32_radians_from_degrees(camera->fov_deg);

  M4F projection = { };


  M4F perspective = m4f_hom_perspective(camera->aspect_ratio, fov_rad, camera->near_m, camera->far_m);
 
  F32 h        = 2.f * camera->radius_m_t * f32_tan(.5f * fov_rad);
  F32 w        = h * camera->aspect_ratio;

  V2F bottom_left = v2f(-.5f * w, -.5f * h);
  V2F top_right   = v2f(+.5f * w, +.5f * h);
  M4F orthographic = m4f_hom_orthographic(bottom_left, top_right, 0, 0);
  
  projection = m4f_lerp(camera->orthographic_t, perspective, orthographic);
  return projection;
}

fn_internal void draw_viewport(UI_Response *response, R2F draw_region, void *user_data) {
  V2F position = draw_region.min;
  V2F size     = v2f_sub(draw_region.max, draw_region.min);

  g2_draw_rect(position, size, .color = v4f(.2f, .2f, .3f, 1));
  g2_submit_draw();

  if (platform_input()->mouse.left.down) {
    camera.theta_deg += 10.f * platform_display()->frame_delta * platform_input()->mouse.position_dt.x;
    camera.phi_deg   += 10.f * platform_display()->frame_delta * platform_input()->mouse.position_dt.y;
  }

  camera.radius_m += platform_input()->mouse.scroll_dt.y * .025f;

  camera_update(&camera, draw_region);
  M4F view = camera_view(&camera);
  M4F projection = camera_projection(&camera);
  M4F world_view_projection = m4f_mul(view, projection);

  R_Constant_Buffer_World_3D test_world = {
    .World_View_Projection = world_view_projection,
    .Eye_Position          = v3f(0, 0, 0),
  };

  world_buffer = r_buffer_allocate(sizeof(R_Constant_Buffer_World_3D), R_Buffer_Mode_Static);
  r_buffer_download(world_buffer, 0, sizeof(test_world), &test_world);

#if 1

  R2I pixel_draw_region = r2i(draw_region.x0, draw_region.y0, draw_region.x1, draw_region.y1);

  if (loaded_model) {
    R_Command_Draw draw_model = {
      .constant_buffer  = world_buffer,
      .vertex_buffer    = model_vertex_buffer,
      .index_buffer     = model_index_buffer,
      .pipeline         = model_pipeline,
      .texture          = R_Texture_White,
      .sampler          = R_Sampler_Linear_Clamp,

      .draw_index_count  = model_index_count,
      .draw_index_offset = 0,

      .depth_test        = 1,
      .draw_region       = pixel_draw_region,
      .clip_region       = pixel_draw_region,
    };

    r_command_push_draw(&draw_model);
  }

  R_Command_Draw draw_grid = {
    .constant_buffer  = world_buffer,
    .vertex_buffer    = vertex_buffer,
    .index_buffer     = index_buffer,
    .pipeline         = pipeline,
    .texture          = R_Texture_White,
    .sampler          = R_Sampler_Linear_Clamp,

    .draw_index_count = 6,
    .draw_index_offset = 0,

    .depth_test        = 1,
    .draw_region       = pixel_draw_region,
    .clip_region       = pixel_draw_region,
  };

  r_command_push_draw(&draw_grid);
#endif
}

fn_internal void next_frame(B32 first_frame, Platform_Render_Context *render_context) {
  If_Unlikely(first_frame) {
    r_init(render_context);
    g2_init();

    Codepoint icon_codepoints[] = {
      codepoint_from_utf8(str_lit(ICON_FA_FILE), 0),
      codepoint_from_utf8(str_lit(ICON_FA_PAUSE), 0),
      codepoint_from_utf8(str_lit(ICON_FA_PLAY), 0),
      codepoint_from_utf8(str_lit(ICON_FA_CUBE), 0),
      codepoint_from_utf8(str_lit(ICON_FA_FORWARD), 0),
      codepoint_from_utf8(str_lit(ICON_FA_FORWARD_FAST), 0),
      codepoint_from_utf8(str_lit(ICON_FA_FORWARD_STEP), 0),
      codepoint_from_utf8(str_lit(ICON_FA_FONT), 0),
      codepoint_from_utf8(str_lit(ICON_FA_EYE), 0),
    };

    fo_font_init(&UI_Font_Text, &Permanent_Storage,
                 str(figtree_regular_ttf_len, figtree_regular_ttf),
                 26, v2_u16(512, 512), Codepoints_ASCII);

    fo_font_init(&UI_Font_Icon, &Permanent_Storage,
                 str(Font_Awesome_7_Free_Solid_900_otf_len, Font_Awesome_7_Free_Solid_900_otf),
                 26, v2_u16(512, 512), array_from_sarray(Array_Codepoint, icon_codepoints));

    ui_init(&UI_Font_Text);

    arena_init(&request_arena);
    http_request_send(&request, &request_arena, str_lit("stanford_dragon.stl"));

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

    pipeline       = r_pipeline_create(R_Shader_Grid_3D, &R_Vertex_Format_XUC_3D, 1);
    model_pipeline = r_pipeline_create(R_Shader_Flat_3D, &R_Vertex_Format_XUC_3D, 1);

    vertex_buffer = r_buffer_allocate(sizeof(test_vertices), R_Buffer_Mode_Static);
    r_buffer_download(vertex_buffer, 0, sizeof(test_vertices), test_vertices);

    index_buffer = r_buffer_allocate(sizeof(test_indices), R_Buffer_Mode_Static);
    r_buffer_download(index_buffer, 0, sizeof(test_indices), test_indices);
  }

  if (!loaded_model && request.status == HTTP_Status_Done) {
    loaded_model = 1;

    U32 tri_count = 0;
    R_Vertex_XUC_3D *vertices = stl_parse_binary(&request_arena, request.bytes_total, request.bytes_data, &tri_count);
    log_info("Loaded STL: %u triangles", tri_count);

    model_vertex_buffer = r_buffer_allocate(3 * sizeof(R_Vertex_XUC_3D) * tri_count, R_Buffer_Mode_Static);
    r_buffer_download(model_vertex_buffer, 0, 3 * sizeof(R_Vertex_XUC_3D) * tri_count, vertices);

    U32 *indices = (U32 *)arena_push_size(&request_arena, 3 * sizeof(U32) * tri_count);
    For_U32 (it, 3 * tri_count) {
      indices[it] = it;
    }

    model_index_buffer = r_buffer_allocate(3 * sizeof(U32) * tri_count, R_Buffer_Mode_Static);
    model_index_count  = 3 * tri_count;
    r_buffer_download(model_index_buffer, 0, 3 * sizeof(U32) * tri_count, indices);
  }

  ui_frame_begin();

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
      ui_checkbox(str_lit("orthographic view"), &camera.orthographic);
    }
  }
 
  if (context_menu && platform_input()->mouse.left.down_first_frame) {
    context_menu = 0;
  }

  UI_Node *workspace = ui_container(str_lit("workspace"), UI_Container_Mode_Box, Axis2_X, UI_Size_Fill, UI_Size_Fill);
  workspace->palette.idle = v3f(.8f, .8f, .8f);
  workspace->palette.hover = v3f(.7f, .6f, .8f);
  UI_Parent_Scope(workspace) {
    UI_Node *layers = ui_container(str_lit("layers"), UI_Container_Mode_Box, Axis2_Y, UI_Size_Fixed(400), UI_Size_Fill);
    layers->palette.idle = v3f(.5f, .8f, .9f);
    layers->palette.hover = v3f(.4f, .6f, .9f);

    UI_Parent_Scope(layers) {

      UI_Parent_Scope(ui_container(str_lit("menu_bar"), UI_Container_Mode_Box, Axis2_X, UI_Size_Fill, UI_Size_Fit)) {
        ui_button(str_lit("View"));
        ui_button(str_lit("Filter"));
      }


      UI_Node *entry_2 = ui_container(str_lit("entry_2"), UI_Container_Mode_Box, Axis2_X, UI_Size_Fill, UI_Size_Fit);
      UI_Parent_Scope(entry_2) {
        UI_Font_Scope(&UI_Font_Icon) { ui_label(str_lit(ICON_FA_FONT)); }
        ui_label(str_lit("Title"));
        ui_container(str_lit("center_padding"), UI_Container_Mode_None, Axis2_X, UI_Size_Fill, UI_Size_Fit);
        UI_Font_Scope(&UI_Font_Icon) { ui_button(str_lit(ICON_FA_EYE)); }
      }

      UI_Node *entry_1 = ui_container(str_lit("entry_1"), UI_Container_Mode_Box, Axis2_X, UI_Size_Fill, UI_Size_Fit);
      UI_Parent_Scope(entry_1) {
        UI_Font_Scope(&UI_Font_Icon) { ui_label(str_lit(ICON_FA_CUBE)); }
        ui_label(str_lit("Dragon"));
        ui_container(str_lit("center_padding"), UI_Container_Mode_None, Axis2_X, UI_Size_Fill, UI_Size_Fit);
        UI_Font_Scope(&UI_Font_Icon) { ui_button(str_lit(ICON_FA_EYE)); }
      }
 
    }

    UI_Node *viewport = ui_container(str_lit("viewport"), UI_Container_Mode_Box, Axis2_Y, UI_Size_Fill, UI_Size_Fill);
    viewport->palette.idle = v3f(.1f, .8f, .9f);
    viewport->palette.hover = v3f(.0f, .6f, .9f);

    UI_Parent_Scope(viewport) {
      UI_Parent_Scope(ui_container(str_lit("menu_bar"), UI_Container_Mode_Box, Axis2_X, UI_Size_Fill, UI_Size_Fit)) {
        ui_button(str_lit("View"));
        ui_button(str_lit("Render"));
      }

      UI_Node *draw_region = ui_container(str_lit("draw_region"), UI_Container_Mode_Box, Axis2_X, UI_Size_Fill, UI_Size_Fill);
      draw_region->flags |= UI_Flag_Draw_Content_Hook;
      draw_region->draw.content_hook = draw_viewport;
    }

  }


#if 0
  g2_clip_region(G2_Clip_None);

  fps_ring[fps_at] = f32_div_safe(1, platform_display()->frame_delta);
  fps_at = (fps_at + 1) % sarray_len(fps_ring);

  F32 fps_avg = 0;
  F32 fps_max = 0;
  For_U32(it, sarray_len(fps_ring)) {
    fps_avg += fps_ring[it];
    fps_max = f32_max(fps_ring[it], fps_max);
  }

  F32 bar_w  = 2.f;
  F32 bar_s  = 1.f;
  F32 offset = (bar_w + bar_s);
  For_U32(it, sarray_len(fps_ring)) {
    g2_draw_rect(v2f(offset * it, 0), v2f(1.f, (fps_ring[it] / fps_max) * 300.f));
  }


  fps_avg /= sarray_len(fps_ring);

  char buffer[512];
  stbsp_snprintf(buffer, 512, "%.2f", fps_avg);

  g2_draw_text(str_from_cstr(buffer), &UI_Font_Text, v2f(10, 300));
#endif

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

