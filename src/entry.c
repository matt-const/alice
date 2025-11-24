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

#include "image/image_build.h"
#include "image/image_build.c"

#if 0

typedef struct RPack_Skyline {
  V2_U16 atlas_size;

  U32    points_cap;
  U32    points_len;
  U16   *points_x;
  U16   *points_y;
} RPack_Skyline;

cb_function RPack_Skyline rpack_skyline_init(Arena *arena, V2_U16 atlas_size) {
  RPack_Skyline sl = {
    .points_cap = atlas_size.w,
    .points_len = 0,
    .points_x   = arena_push_count(&arena, U16, atlas_size.w),
    .points_y   = arena_push_count(&arena, U16, atlas_size.w),
  };

  return sl;
}

cb_function B32 rpack_skyline_push(RPack_Skyline *sl, V2I rect, I32 border, R2I *region) {
  rect.x1 += border;
  rect.y1 += border;

  U32 lowest_index = 0;
  U16 lowest       = u16_max;
  For_U32(it, sl->points_len) {
    if (sl->points_x[it] + rect.w  > sl->atlas_size.x) {
      break;
    }

    if ((sl->points_y[it] < lowest) &&
        (sl->points_y[it] + rect.h <= sl->atlas_size.y)) {


    }
  }

  return 0;
}

#endif


cb_function void next_frame(B32 first_frame, Platform_Render_Context *render_context) {
  If_Unlikely(first_frame) {

    Scratch scratch = { };
    Scratch_Scope(&scratch, 0) {
      IM_Bitmap bitmap = im_bitmap_allocate(scratch.arena, 1024, 1024, 3);

      Random_Seed rng = 1234;
      For_U64 (it, 1024 * 1024) {
        bitmap.dat[3 * it + 0] = random_next(&rng) % 256;
        bitmap.dat[3 * it + 1] = random_next(&rng) % 2 * 100;
        bitmap.dat[3 * it + 2] = random_next(&rng) % 100 * 2;
      }

      im_bitmap_write_file(&bitmap, str_lit("bitmap.tga"), IM_File_Format_TGA);
    }
  }
}

cb_function void log_hardware_info(void) {
  Log_Zone_Scope("hardware info") {
    log_info("CPU: %.*s",               str_expand(core_context()->cpu_name));
    log_info("Logical Cores: %llu",     core_context()->cpu_logical_cores);
    log_info("Page Size: %$$llu",       core_context()->mmu_page_bytes);
    log_info("RAM Capacity: %$$llu",    core_context()->ram_capacity_bytes);
  }
}

cb_function void logger_write_entry_file (Logger_Entry_Type type, Str buffer) {
  Core_File file = { };
  File_IO_Scope(&file, str_lit("log.txt"), Core_File_Access_Flag_Write | Core_File_Access_Flag_Append | Core_File_Access_Flag_Create) {
    core_file_write(&file, 0, buffer.len, buffer.txt);
  }
}

cb_function void platform_entry_point(Array_Str command_line, Platform_Bootstrap *boot) {
  boot->next_frame = next_frame;
  boot->title = str_lit("Genome - V2 DEMO");

  // NOTE(cmat): Reset log file.
  Core_File file = { };
  File_IO_Scope(&file, str_lit("log.txt"), Core_File_Access_Flag_Truncate) { }

  logger_push_hook(logger_write_entry_standard_stream, logger_format_entry_minimal);
  logger_push_hook(logger_write_entry_file,            logger_format_entry_detailed);
  log_hardware_info();

  Log_Zone_Scope("testing all subsystems") {
    test_base_all();
  }

  Log_Zone_Scope("testing complete!");
} 
