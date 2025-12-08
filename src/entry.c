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

#include "geometry/geometry_build.h"
#include "geometry/geometry_build.c"

Random_Seed RNG = 1234;

#define BIN_COUNT 2000

Arena packer_arena = { };
Skyline_Packer sk = { };

U32 bin_at = 0;
V2F bin_positions[BIN_COUNT] = { };
V2F bin_sizes    [BIN_COUNT] = { };
V4F bin_colors   [BIN_COUNT] = { };

cb_function void next_frame(B32 first_frame, Platform_Render_Context *render_context) {
  If_Unlikely(first_frame) {
    r_init(render_context);
    g2_init();

    arena_init(&packer_arena);
    skyline_packer_init(&sk, &packer_arena, v2i(1800, 1800));
  }

  if (platform_input()->mouse.left.down_first_frame) {
    bin_at = 0;
    skyline_packer_reset(&sk);
    log_info("reset");
  }

  if (bin_at < BIN_COUNT) {
    V2_U16 rect = v2_u16(5 + (U16)(random_next(&RNG) % 50), 5 + (U16)(random_next(&RNG) % 50));
    V2_U16 pos  = { };
    skyline_packer_push(&sk, rect, 5, &pos);
    bin_positions[bin_at] = v2f(pos.x, pos.y);
    bin_sizes[bin_at]     = v2f(rect.x, rect.y);
    bin_colors[bin_at].xyz = rgb_from_hsv(v3f(f32_random_unilateral(&RNG), .5f, .8f + .2f * f32_random_unilateral(&RNG)));
    bin_colors[bin_at].a   = 1.f;

    bin_at++;
  }

  if (bin_at == BIN_COUNT) {
    log_info("skyline size: %llu", sk.nodes.len);
  }
  
  For_U32(it, bin_at) {
    g2_draw_rect(bin_positions[it], bin_sizes[it], .color = bin_colors[it]);
  }

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

