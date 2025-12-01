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

#if 0
#include "platform/platform_build.h"
#include "platform/platform_build.c"

#include "image/image_build.h"
#include "image/image_build.c"

#include "render/render_build.h"
#include "render/render_build.c"

#include "font/font_build.h"
#include "font/font_build.c"

#include "graphics/graphics_build.h"
#include "graphics/graphics_build.c"
#endif

cb_function void log_hardware_info(void) {
  Log_Zone_Scope("hardware info") {
      log_info("CPU: %.*s",               str_expand(core_context()->cpu_name));
      log_info("Logical Cores: %llu",     core_context()->cpu_logical_cores);
      log_info("Page Size: %$$llu",       core_context()->mmu_page_bytes);
      log_info("RAM Capacity: %$$llu",    core_context()->ram_capacity_bytes);
  }
  
  Assert(0, "assertion test");
}

cb_function void base_entry_point(Array_Str command_line) {
  logger_push_hook(logger_write_entry_standard_stream, logger_format_entry_detailed);
  log_hardware_info();

  Log_Zone_Scope("testing all subsystems") {
    test_base_all();
  }

  Log_Zone_Scope("testing complete!");
}


#if 0

cb_function void next_frame(B32 first_frame, Platform_Render_Context *render_context) {
    If_Unlikely(first_frame) {
        r_init(render_context);
        g2_init();
    }
    
    g2_draw_rect(v2f(0, 0), v2f(100, 100), .color = v4f(1, 0, 0, 1));
    
    g2_frame_flush();
    r_frame_flush();
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

#endif
