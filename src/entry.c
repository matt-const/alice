// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

// TODO(cmat): 
// - 1. Bin-packing implementation.
// - 2. Freetype font rasterization.
// - 3. UI rounding corners/

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

cb_global R_Buffer   Constant_Buffer  = R_Resource_None;
cb_global R_Buffer   Vertex_Buffer    = R_Resource_None;
cb_global R_Buffer   Index_Buffer     = R_Resource_None;
cb_global R_Pipeline Pipeline         = R_Resource_None;

cb_function void next_frame(B32 first_frame, Platform_Render_Context *render_context) {
  If_Unlikely(first_frame) {
    r_init(render_context);
    g2_init();
  }

  g2_draw_rect(v2f(0, 0),     v2f(500, 500), .color = v4f(.2f, .8f, .2f, 1.f));
  g2_draw_rect(v2f(250, 250), v2f(500, 500), .color = v4f(.8f, .2f, .2f, 1.f));
  g2_draw_rect(v2f(400, 400), v2f(500, 500), .color = v4f(.8f, .8f, .2f, 1.f));

  g2_draw_rect(platform_input()->mouse.position, v2f(500, 500), .color = v4f(.2f, .2f, .8f, 1.f));
  
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

