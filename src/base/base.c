// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

// ------------------------------------------------------------
// #-- Arena

fn_internal U08 *arena_chunk_allocate(Arena_Chunk *chunk, U64 bytes, Arena_Push *alloc) {
  U08 *alloc_begin  = pointer_align(chunk->current, alloc->align);
  U08 *alloc_end    = alloc_begin + bytes;

  // NOTE(cmat): If we overstep the boundary of the current page,
  // - commit a new page.
  if (alloc_end > chunk->next_page) {

    // NOTE(cmat): If we run out of reserved memory, return zero to signal
    // - that a new chunk should be allocated.
    if ((alloc_end - chunk->base_memory) >= chunk->reserved) {
      return 0;
    } else {
      U64 page_bytes = core_context()->mmu_page_bytes;
      U64 grow_bytes = address_align((U64)(alloc_end - chunk->next_page), page_bytes);
  
      core_memory_commit(chunk->next_page, grow_bytes, Core_Commit_Flag_Read | Core_Commit_Flag_Write);
      chunk->next_page += grow_bytes;
    }
  }

  chunk->current = alloc_end;

  // NOTE(cmat): Zero-initialize memory, if requested.
  if (alloc->flags & Arena_Push_Flag_Zero_Init) {
    memory_fill(alloc_begin, 0, bytes);
  }

  return alloc_begin;
}

fn_internal void arena_chunk_deallocate(Arena_Chunk *chunk) {
  Arena_Chunk chunk_header = {
    .header = {
#if BUILD_DEBUG
      .magic        = arena_chunk_magic,
#endif
    },

    .base_memory    = chunk->base_memory,
    .next_page      = chunk->base_memory,
    .current        = chunk->base_memory,
    .reserved       = chunk->reserved,
    .prev           = chunk->prev,
    .next           = chunk->next,
  };

  core_memory_uncommit(chunk->base_memory, chunk->next_page - chunk->base_memory);
  
  Arena_Push header_alloc = { .align = Arena_Alignment_Default, .flags = 0 };
  Arena_Chunk *new_header = (Arena_Chunk *)arena_chunk_allocate(&chunk_header, sizeof(Arena_Chunk), &header_alloc);
  memory_copy(new_header, &chunk_header, sizeof(Arena_Chunk)); 
}

fn_internal Arena_Chunk *arena_chunk_init(Arena_Chunk *prev, U64 reserve_bytes) {
  U64 page_bytes = core_context()->mmu_page_bytes;
  
  reserve_bytes = reserve_bytes + sizeof(Arena_Chunk);
  reserve_bytes = address_align(reserve_bytes, page_bytes);

  Arena_Chunk chunk;
  zero_fill(&chunk);
  
#if BUILD_DEBUG
  chunk.header.magic = arena_chunk_magic;
#endif

  chunk.base_memory = core_memory_reserve(reserve_bytes);
  chunk.next_page   = chunk.base_memory;
  chunk.reserved    = reserve_bytes;
  chunk.current     = chunk.base_memory;
  chunk.prev        = prev;
  chunk.next        = 0;
  
  // NOTE(cmat): Push chunk header first.
  Arena_Push header_alloc = { .align = Arena_Alignment_Default, .flags = 0 };
  Arena_Chunk *header = (Arena_Chunk *)arena_chunk_allocate(&chunk, sizeof(chunk), &header_alloc);
  memory_copy(header, &chunk, sizeof(Arena_Chunk));

  if (prev) {
    prev->next = header;
  }

  return (Arena_Chunk *)header;
}

fn_internal Arena_Chunk *arena_chunk_destroy(Arena_Chunk *chunk) {
  Arena_Chunk *prev = chunk->prev;
  if (prev) {
    chunk->prev->next = 0;
  }
  
  U08 *base_memory    = chunk->base_memory;
  U64 uncommit_bytes  = chunk->next_page - chunk->base_memory;
  U64 unreserve_bytes = chunk->reserved;
  
  core_memory_uncommit  (base_memory, uncommit_bytes);
  core_memory_unreserve (base_memory, unreserve_bytes);

  return prev;
}

fn_internal void arena_init_ext(Arena *arena, Arena_Init *config) {
  Assert(!arena->first_chunk && !arena->last_chunk, "reinitializing arena");
  
  zero_fill(arena);
  arena->flags = config->flags;
 
  if (config->reserve_initial) {
    arena->first_chunk = arena_chunk_init(0, config->reserve_initial);
    arena->last_chunk = arena->first_chunk; 
  }
}

fn_internal void arena_destroy(Arena *arena) {
  Arena_Chunk *it = arena->last_chunk;
  while (it) it = arena_chunk_destroy(it);
  zero_fill(arena);  
}


// NOTE(cmat): Try pushing an allocation to the current chunk.
// - If the chunk doesn't have enough reserved virtual memory,
// - create a new chunk and chain.
// -
// - This may seem counterintuitive at first; after all, if we're
// - creating a a new chunk, surely we must be leaving gaps in memory.
// - The trick however is, the range in the chunk is only reserved, comitted.
// - This means, in a situation such as:
// -
// - [ ******-------- ] -> [ ************************ ]
// -      OLD CHUNK               NEW CHUNK
// -
// - The region marked with `-` is reserved by the MMU, but not actually comitted
// - to phsyical memory space.
// - Also to note, if the new allocation size to chain is less than the chunk size,
// - the chunk size is allocated.
//
// - [ ******-------- ] -> [ ***********--- ]
// -      OLD CHUNK            NEW CHUNK
// -
// - In cases where virtual memory is limitted or non-existant
// - like WASM, you can use the flag Arena_Flag_Backtrack_Before_Chaining.
// - This iterates through older chunks and try to place our allocations there.
// - Example:
// -
// - [ ******-------- ] -> [ ***********--- ]
// -      OLD CHUNK            NEW CHUNK
// -
// - We have a new allocation, '*****'.
// - We backtrack through our doubly linked list, until we find a chunk
// - that doesn't fail to allocate.
// -
// - [ ******-------- ] -> [ ***********--- ]
// -         ^ insert new alloc here.
// -      OLD CHUNK            NEW CHUNK

fn_internal U08 *arena_allocate_within_new_chunk(Arena *arena, U64 bytes, Arena_Push *config) {
    U64 default_chunk_bytes = arena_default_chunk_bytes;
    if (sizeof(Arena_Chunk) < default_chunk_bytes) {
      default_chunk_bytes -= sizeof(Arena_Chunk);
    }

    U64 chunk_reserve = bytes > default_chunk_bytes ? bytes : default_chunk_bytes;

    arena->last_chunk = arena_chunk_init(arena->last_chunk, chunk_reserve);
    Assert(arena->last_chunk, "failed to allocate new chunk");
    
    U08 *user_allocation = arena_chunk_allocate(arena->last_chunk, bytes, config);
    Assert(user_allocation, "failed to allocate memory");

    return user_allocation;
}

fn_internal U08 *arena_push_ext(Arena *arena, U64 bytes, Arena_Push *config) {
  U64 default_chunk_bytes = arena_default_chunk_bytes;
  if (sizeof(Arena_Chunk) < default_chunk_bytes) {
    default_chunk_bytes -= sizeof(Arena_Chunk);
  }

  U64 chunk_reserve = bytes > default_chunk_bytes ? bytes : default_chunk_bytes;
  
  // NOTE(cmat): Initialize first chunk if the arena is empty.
  // - This branch is predictable after intialization.
  If_Unlikely(!arena->first_chunk) {
    arena->first_chunk = arena_chunk_init(0, chunk_reserve);
    arena->last_chunk = arena->first_chunk; 
  }
 
  U08 *user_allocation = arena_chunk_allocate(arena->last_chunk, bytes, config);
  if (!user_allocation) {
    if (arena->flags & Arena_Flag_Allow_Chaining) {
      if (arena->flags & Arena_Flag_Backtrack_Before_Chaining) {

        // TODO(cmat): In pathological cases, this seems bad since we're iterating at worst
        // - N-times before finding an open slot.
        // - My gut feeling is we can say the same thing here, as in an open-slot hash-table,
        // - where if you have to iterate through the whole table, your hash is definitely
        // - terrible. If you're iterating this much through your arena chunks, you definitely did
        // - a bad job managing the arena's lifetime. Maybe logging a warning or straight up throwing
        // - an assert at N > 10 could be ok.
        for (Arena_Chunk *it = arena->last_chunk->prev; it != 0; it = it->prev) {
          user_allocation = arena_chunk_allocate(it, bytes, config);
          if (user_allocation) break;
        }

        // NOTE(cmat): If no chunks were open, we chain.
        if (!user_allocation)
          user_allocation = arena_allocate_within_new_chunk(arena, bytes, config);
      } else {
        user_allocation = arena_allocate_within_new_chunk(arena, bytes, config);
      }
    } else {
      Assert(0, "arena exceeded reserved limit, and does not have chaining enabled");
    }
  }

  return user_allocation;
}

fn_internal void arena_clear(Arena *arena) {
  // NOTE(cmat): We destroy all chunks, but only deallocate the first one.
  // - If you wish to destroy the first chunk as well, call arena_destroy
  // - (althought that does require calling arena_init again).
  if (arena->last_chunk) {
    Arena_Chunk *it = arena->last_chunk;
    while (it != arena->first_chunk) {
      it = arena_chunk_destroy(it);
    }
    arena_chunk_deallocate(it);
    arena->last_chunk = arena->first_chunk;
  }
}

fn_internal Arena_Temp arena_temp_start(Arena *arena) {
  Arena_Temp result;
  if (!arena->last_chunk) {
    result = (Arena_Temp) { .arena = arena };
  } else {
    result = (Arena_Temp) {
        .arena = arena,
        .rollback_chunk   = arena->last_chunk,
        .rollback_current = arena->last_chunk->current,
        .rollback_page    = arena->last_chunk->next_page
    };
  }

  return result;
}

fn_internal void arena_temp_end(Arena_Temp *temporary) {
  if (!temporary->rollback_chunk) {
    arena_clear(temporary->arena);
    zero_fill(temporary);
  } else {
    Arena_Chunk *it = temporary->arena->last_chunk;
    while (it != temporary->rollback_chunk) it = arena_chunk_destroy(it);

    core_memory_uncommit(temporary->rollback_page, it->next_page - temporary->rollback_page);
    it->next_page = temporary->rollback_page;
    it->current = temporary->rollback_current;
   
    temporary->arena->last_chunk = temporary->rollback_chunk;
    zero_fill(temporary);
  }
}

thread_local struct {
  Arena scratch_1;
  Arena scratch_2;
} Scratch_Thread;

fn_internal Arena *scratch_get_for_thread(Arena *conflict) {
  Arena *scratch = &Scratch_Thread.scratch_1;
  If_Unlikely(conflict == &Scratch_Thread.scratch_1) {
    scratch = &Scratch_Thread.scratch_2;
  }
  
  return scratch;
}

fn_internal void scratch_init_for_thread(void) {
  arena_init(&Scratch_Thread.scratch_1);
  arena_init(&Scratch_Thread.scratch_2);
}

// ------------------------------------------------------------
// #-- Hash Tables

#if 0
fn_internal void hash_table_init(Hash_Table *ht, Arena *arena, U64 bucket_type_bytes, U64 bucket_count) {
  zero_fill(ht);
  ht->bucket_type_bytes = bucket_type_bytes;
  ht->bucket_list_count = bucket_count;
  ht->bucket_list_array = arena_push_count(arena, Bucket_List, bucket_count);
}

fn_internal void hash_table_get_or_create()

#endif

// ------------------------------------------------------------
// #-- Logging

var_global Logger_State Logger = {
  .filter       = Logger_Filter_Build_Active,
  .zone_depth   = 0,
  .hook_count   = 0,
  .write_hooks  = { },
  .format_hooks = { },
  .mutex        = { },
};

fn_internal void logger_entry(Logger_Entry *entry) {
  thread_local var_local_persist U08 entry_buffer[Logger_Max_Entry_Length] = {};

  Mutex_Scope(&Logger.mutex) {

      // TODO(cmat): This is such a mess. Why are we checking this here again?
      If_Likely ((Logger.filter & logger_filter_flag_from_entry_type(entry->type))) {

      // NOTE(cmat): Zone-ing.
      if (entry->type == Logger_Entry_Zone_Start) {
          Logger.zone_depth += 1;
      } else if (entry->type == Logger_Entry_Zone_End) {
          Assert(Logger.zone_depth, "unmatched log_zone_start / log_zone_end calls");
          if (Logger.zone_depth) Logger.zone_depth -= 1;
      }

      // NOTE(cmat): Call all format/write hooks.
      For_I32(it, Logger.hook_count) {
          Logger.format_hooks[it](entry, entry_buffer, Logger.zone_depth);
          if (*entry_buffer)
          Logger.write_hooks[it](entry->type, str_from_cstr((char *)entry_buffer));
      }
    }
  }
}

fn_internal void logger_set_filter(Logger_Filter_Flag filter) { 
  Mutex_Scope(&Logger.mutex) {
    Logger.filter = filter;
  }
}

fn_internal B32 logger_filter_type(Logger_Entry_Type type) {
  B32 result = 0;
  Mutex_Scope(&Logger.mutex) {
    result = (Logger.filter & logger_filter_flag_from_entry_type(type));
  }

  return result;
}

fn_internal void logger_push_hook(Logger_Write_Entry_Hook *write, Logger_Format_Entry_Hook *format) {
  Mutex_Scope(&Logger.mutex) {
    Assert(Logger.hook_count < Logger_Max_Hooks, "exceeded hook count");
    Logger.write_hooks[Logger.hook_count] = write;
    Logger.format_hooks[Logger.hook_count] = format;
    Logger.hook_count++;
  }
}

// NOTE(cmat): Default hooks.
// TODO(cmat): Cleanup code once we introduce better string formatting stuff.
// #--
// 

fn_internal void logger_format_entry_minimal(Logger_Entry *entry, U08 *entry_buffer, U32 zone_depth) {
    var_local_persist Str type_lookup[] = {
     str_lit(""), // NOTE(cmat): Info.
     str_lit(" [Debug]: "),
     str_lit(" [Warning]: "),
     str_lit(" [Error]: "),
     str_lit(" [Fatal]: "),
     str_lit(" [Zone_Start]: "),
     str_lit(" [Zone_End]: "),
   };
  
   Str type_str = type_lookup[entry->type];  
   Assert_Compiler(Logger_Entry_Type_Count == sarray_len(type_lookup));

   U32 buffer_at = 0;
   U32 zone_indent = u32_min(5, zone_depth);
   if (entry->type == Logger_Entry_Zone_Start && zone_indent) zone_indent -= 1;
   for (U32 index = 0; index < zone_indent; ++index) {
     entry_buffer[buffer_at++] = ' ';
     entry_buffer[buffer_at++] = ' ';
   }
   
   Assert(buffer_at <= Logger_Max_Entry_Length, "logger buffer overflow");
   
   if (entry->type == Logger_Entry_Zone_Start) {
     stbsp_snprintf((char *)entry_buffer + buffer_at, Logger_Max_Entry_Length - buffer_at, "# %s\n", entry->message);
   } else if (entry->type == Logger_Entry_Zone_End) {
     entry_buffer[0] = 0;
   } else if (entry->type == Logger_Entry_Debug) {
     stbsp_snprintf((char *)entry_buffer + buffer_at, Logger_Max_Entry_Length - buffer_at, "%.*s<%.*s:%d, %.*s>: %s\n",
                     (I32)type_str.len, type_str.txt,
                     (I32)entry->meta.filename.len, entry->meta.filename.txt, entry->meta.line, (I32)entry->meta.function.len, entry->meta.function.txt,
                     entry->message);
   } else {
     stbsp_snprintf((char *)entry_buffer + buffer_at, Logger_Max_Entry_Length - buffer_at, "%.*s%s\n",
                     (I32)type_str.len, type_str.txt, entry->message);
   }
}

fn_internal void logger_format_entry_detailed(Logger_Entry *entry, U08 *entry_buffer, U32 zone_depth) {
    var_local_persist Str type_lookup[] = {
     str_lit(""), // NOTE(cmat): Info.
     str_lit(" [Debug]"),
     str_lit(" [Warning]"),
     str_lit(" [Error]"),
     str_lit(" [Fatal]"),
     str_lit(" [Zone_Start]"),
     str_lit(" [Zone_End]"),
   };
  
   Str type_str = type_lookup[entry->type];  
   Assert_Compiler(Logger_Entry_Type_Count == sarray_len(type_lookup));

   U32 buffer_at = 0;
   U32 zone_indent = u32_min(5, zone_depth);
   if (entry->type == Logger_Entry_Zone_Start && zone_indent) zone_indent -= 1;
   for (U32 index = 0; index < zone_indent; ++index) {
     entry_buffer[buffer_at++] = '|';
     entry_buffer[buffer_at++] = ' ';
   }
   
   Assert(buffer_at <= Logger_Max_Entry_Length, "logger buffer overflow");
   
   if (entry->type == Logger_Entry_Zone_Start) {
     stbsp_snprintf((char *)entry_buffer + buffer_at, Logger_Max_Entry_Length - buffer_at, "# %s\n", entry->message);
   } else if (entry->type == Logger_Entry_Zone_End) {
     entry_buffer[0] = 0;
   } else if (entry->type == Logger_Entry_Debug) {
     stbsp_snprintf((char *)entry_buffer + buffer_at, Logger_Max_Entry_Length - buffer_at, "(%02d:%02d:%02d %s %02d/%02d/%02d)%.*s <%.*s:%d, %.*s>: %s\n",
                     entry->time.hours > 12 ? entry->time.hours - 12 : entry->time.hours, entry->time.minutes, entry->time.seconds,
                     entry->time.hours > 12 ? "PM" : "AM",
                     entry->time.day, entry->time.month, entry->time.year,
                     (I32)type_str.len, type_str.txt,
                     (I32)entry->meta.filename.len, entry->meta.filename.txt, entry->meta.line, (I32)entry->meta.function.len, entry->meta.function.txt,
                     entry->message);
   } else {
     stbsp_snprintf((char *)entry_buffer + buffer_at, Logger_Max_Entry_Length - buffer_at, "(%02d:%02d:%02d %s %02d/%02d/%02d)%.*s: %s\n",
                     entry->time.hours > 12 ? entry->time.hours - 12 : entry->time.hours, entry->time.minutes, entry->time.seconds,
                     entry->time.hours > 12 ? "PM" : "AM",
                     entry->time.day, entry->time.month, entry->time.year,
                     (I32)type_str.len, type_str.txt, entry->message);
   }
}
fn_internal void logger_write_entry_standard_stream(Logger_Entry_Type type, Str buffer) {
  switch (type) {
    case Logger_Entry_Error:
    case Logger_Entry_Fatal: {
        core_stream_write(buffer, Core_Stream_Standard_Error);
    } break;

    case Logger_Entry_Warning: {
        core_stream_write(buffer, Core_Stream_Standard_Error);
    } break;

    default: {
        core_stream_write(buffer, Core_Stream_Standard_Output);
    } break;
  }
}

fn_internal void log_message_ext(Logger_Entry_Type type, Function_Metadata func_meta, char *format, ...) {
  if (logger_filter_type(type)) {
    Logger_Entry entry = { 
      .type = type, 
      .time = core_local_time(), 
      .meta = func_meta,
    }; 

    va_list args;
    va_start(args, format);
    entry.message[stbsp_vsnprintf((char *)entry.message, Logger_Max_Entry_Length, format, args)] = 0;
    va_end(args);

    logger_entry(&entry);
  }
}

// ------------------------------------------------------------
// #-- Color Spaces

fn_internal RGB rgb_from_hsv(HSV hsv) {

  // NOTE(cmat): Compute hue only (assume saturation = 1, value = 1)
  F32 h_prime = 6.f * hsv.h;
  V3F hue_rgb = v3f_abs(v3f_sub(v3f_f32(h_prime), v3f(3.f, 2.f, 4.f)));
  hue_rgb = v3f(hue_rgb.r - 1.f, 2.f - hue_rgb.g, 2.f - hue_rgb.b);
  hue_rgb = v3f_saturate(hue_rgb);

  // NOTE(cmat): Apply saturation and value.
  V3F hsv_rgb = v3f_mul(hsv.v, v3f_add(v3f_mul(hsv.s, v3f_sub(hue_rgb, v3f_f32(1.f))), v3f_f32(1.f)));
  return hsv_rgb; 
}

fn_internal HSV hsv_from_rgb(RGB rgb) {
  Not_Implemented;
  return v3f(0, 0, 0);
}

fn_internal RGBA rgba_from_hsva(HSVA hsva) {
  return (RGBA) { .rgb = rgb_from_hsv(hsva.hsv), .a = hsva.a };
}

fn_internal HSVA hsva_from_rgba(RGBA rgba) {
  return (HSVA) { .hsv = hsv_from_rgb(rgba.rgb), .a = rgba.a };
}

fn_internal RGBA_U32 rgba_u32_from_rgba(RGBA rgba) {
  U32 packed = ((U32)(U08)rgba.r << 24) | ((U32)(U08)rgba.g << 16) | ((U32)(U08)rgba.b <<  8) | ((U32)(U08)rgba.a);
  return packed;
}

fn_internal RGBA_U32 abgr_u32_from_rgba(RGBA rgba) {
  U32 packed = ((U32)(U08)(rgba.a * 255.f) << 24) | ((U32)(U08)(rgba.b * 255.f) << 16) | ((U32)(U08)(rgba.g * 255.f) <<  8) | ((U32)(U08)(rgba.r * 255.f));
  return packed;
}

fn_internal RGBA_U32 rgba_u32_from_rgba_premul(RGBA rgba) {
  rgba.r *= rgba.a;
  rgba.g *= rgba.a;
  rgba.b *= rgba.a;
  return rgba_u32_from_rgba(rgba);
}

fn_internal RGBA_U32 abgr_u32_from_rgba_premul(RGBA rgba) {
  rgba.r *= rgba.a;
  rgba.g *= rgba.a;
  rgba.b *= rgba.a;
  return abgr_u32_from_rgba(rgba);
}

// ------------------------------------------------------------
// #-- Splines

fn_internal V2F v2f_spline_catmull(F32 t, V2F p1, V2F p2, V2F p3, V2F p4) {
  F32 t2 = t * t;
  F32 t3 = t2 * t;

  F32 a = -0.5f * t3 + 1.0f * t2 - 0.5f * t;
  F32 b = +1.5f * t3 - 2.5f * t2 + 1.0f;
  F32 c = -1.5f * t3 + 2.0f * t2 + 0.5f * t;
  F32 d = +0.5f * t3 - 0.5f * t2;

  V2F result =             v2f_mul(a, p1);
  result = v2f_add(result, v2f_mul(b, p2));
  result = v2f_add(result, v2f_mul(c, p3));
  result = v2f_add(result, v2f_mul(d, p4));

  return result;
}

fn_internal V3F v3f_spline_catmull(F32 t, V3F p1, V3F p2, V3F p3, V3F p4) {
  F32 t2 = t * t;
  F32 t3 = t2 * t;

  F32 a = -0.5f * t3 + 1.0f * t2 - 0.5f * t;
  F32 b = +1.5f * t3 - 2.5f * t2 + 1.0f;
  F32 c = -1.5f * t3 + 2.0f * t2 + 0.5f * t;
  F32 d = +0.5f * t3 - 0.5f * t2;

  V3F result =             v3f_mul(a, p1);
  result = v3f_add(result, v3f_mul(b, p2));
  result = v3f_add(result, v3f_mul(c, p3));
  result = v3f_add(result, v3f_mul(d, p4));

  return result;
}

fn_internal V4F v4f_spline_catmull(F32 t, V4F p1, V4F p2, V4F p3, V4F p4) {
  F32 t2 = t * t;
  F32 t3 = t2 * t;

  F32 a = -0.5f * t3 + 1.0f * t2 - 0.5f * t;
  F32 b = +1.5f * t3 - 2.5f * t2 + 1.0f;
  F32 c = -1.5f * t3 + 2.0f * t2 + 0.5f * t;
  F32 d = +0.5f * t3 - 0.5f * t2;

  V4F result =             v4f_mul(a, p1);
  result = v4f_add(result, v4f_mul(b, p2));
  result = v4f_add(result, v4f_mul(c, p3));
  result = v4f_add(result, v4f_mul(d, p4));

  return result;
}

fn_internal V2F v2f_spline_catmull_dt(F32 t, V2F p1, V2F p2, V2F p3, V2F p4) {
  F32 t2 = t * t;

  F32 dt_a = -1.5f * t2 + 2.0f * t - 0.5f;
  F32 dt_b = +4.5f * t2 - 5.0f * t;
  F32 dt_c = -4.5f * t2 + 4.0f * t + 0.5f;
  F32 dt_d = +1.5f * t2 - t;

  V2F result =             v2f_mul(dt_a, p1);
  result = v2f_add(result, v2f_mul(dt_b, p2));
  result = v2f_add(result, v2f_mul(dt_c, p3));
  result = v2f_add(result, v2f_mul(dt_d, p4));

  return result;
}

fn_internal V3F v3f_spline_catmull_dt(F32 t, V3F p1, V3F p2, V3F p3, V3F p4) {
  F32 t2 = t * t;

  F32 dt_a = -1.5f * t2 + 2.0f * t - 0.5f;
  F32 dt_b = +4.5f * t2 - 5.0f * t;
  F32 dt_c = -4.5f * t2 + 4.0f * t + 0.5f;
  F32 dt_d = +1.5f * t2 - t;

  V3F result =             v3f_mul(dt_a, p1);
  result = v3f_add(result, v3f_mul(dt_b, p2));
  result = v3f_add(result, v3f_mul(dt_c, p3));
  result = v3f_add(result, v3f_mul(dt_d, p4));

  return result;
}

fn_internal V4F v4f_spline_catmull_dt(F32 t, V4F p1, V4F p2, V4F p3, V4F p4) {
  F32 t2 = t * t;

  F32 dt_a = -1.5f * t2 + 2.0f * t - 0.5f;
  F32 dt_b = +4.5f * t2 - 5.0f * t;
  F32 dt_c = -4.5f * t2 + 4.0f * t + 0.5f;
  F32 dt_d = +1.5f * t2 - t;

  V4F result =             v4f_mul(dt_a, p1);
  result = v4f_add(result, v4f_mul(dt_b, p2));
  result = v4f_add(result, v4f_mul(dt_c, p3));
  result = v4f_add(result, v4f_mul(dt_d, p4));

  return result;
}

// ------------------------------------------------------------
// #-- Matrix ops

fn_internal F32 m2f_det(M2F x) {
  F32 result = (x.e11 * x.e22) -
               (x.e12 * x.e21);

  return result;
}

fn_internal F32 m3f_det(M3F x) {
  F32 result = (x.e11 * x.e22 * x.e33 +
                x.e12 * x.e23 * x.e31 +
                x.e13 * x.e21 * x.e32) -

               (x.e31 * x.e22 * x.e13 +
                x.e32 * x.e23 * x.e11 + 
                x.e33 * x.e21 * x.e12);

  return result;
}

fn_internal F32 m4f_det(M4F x) {
  F32 result = 
    x.ele[0][3] * x.ele[1][2] * x.ele[2][1] * x.ele[3][0] - x.ele[0][2] * x.ele[1][3] * x.ele[2][1] * x.ele[3][0] -
    x.ele[0][3] * x.ele[1][1] * x.ele[2][2] * x.ele[3][0] + x.ele[0][1] * x.ele[1][3] * x.ele[2][2] * x.ele[3][0] +
    x.ele[0][2] * x.ele[1][1] * x.ele[2][3] * x.ele[3][0] - x.ele[0][1] * x.ele[1][2] * x.ele[2][3] * x.ele[3][0] -
    x.ele[0][3] * x.ele[1][2] * x.ele[2][0] * x.ele[3][1] + x.ele[0][2] * x.ele[1][3] * x.ele[2][0] * x.ele[3][1] +
    x.ele[0][3] * x.ele[1][0] * x.ele[2][2] * x.ele[3][1] - x.ele[0][0] * x.ele[1][3] * x.ele[2][2] * x.ele[3][1] -
    x.ele[0][2] * x.ele[1][0] * x.ele[2][3] * x.ele[3][1] + x.ele[0][0] * x.ele[1][2] * x.ele[2][3] * x.ele[3][1] +
    x.ele[0][3] * x.ele[1][1] * x.ele[2][0] * x.ele[3][2] - x.ele[0][1] * x.ele[1][3] * x.ele[2][0] * x.ele[3][2] -
    x.ele[0][3] * x.ele[1][0] * x.ele[2][1] * x.ele[3][2] + x.ele[0][0] * x.ele[1][3] * x.ele[2][1] * x.ele[3][2] +
    x.ele[0][1] * x.ele[1][0] * x.ele[2][3] * x.ele[3][2] - x.ele[0][0] * x.ele[1][1] * x.ele[2][3] * x.ele[3][2] -
    x.ele[0][2] * x.ele[1][1] * x.ele[2][0] * x.ele[3][3] + x.ele[0][1] * x.ele[1][2] * x.ele[2][0] * x.ele[3][3] +
    x.ele[0][2] * x.ele[1][0] * x.ele[2][1] * x.ele[3][3] - x.ele[0][0] * x.ele[1][2] * x.ele[2][1] * x.ele[3][3] -
    x.ele[0][1] * x.ele[1][0] * x.ele[2][2] * x.ele[3][3] + x.ele[0][0] * x.ele[1][1] * x.ele[2][2] * x.ele[3][3];

  return result;
}

fn_internal B32 m4f_inv(M4F x, M4F *solved) {
  M4F inverse;
  zero_fill(&inverse);

  inverse.dat[0] =  x.dat[5]  * x.dat[10] * x.dat[15] - 
                x.dat[5]  * x.dat[11] * x.dat[14] - 
                x.dat[9]  * x.dat[6]  * x.dat[15] + 
                x.dat[9]  * x.dat[7]  * x.dat[14] +
                x.dat[13] * x.dat[6]  * x.dat[11] - 
                x.dat[13] * x.dat[7]  * x.dat[10];

  inverse.dat[4] = -x.dat[4]  * x.dat[10] * x.dat[15] + 
                x.dat[4]  * x.dat[11] * x.dat[14] + 
                x.dat[8]  * x.dat[6]  * x.dat[15] - 
                x.dat[8]  * x.dat[7]  * x.dat[14] - 
                x.dat[12] * x.dat[6]  * x.dat[11] + 
                x.dat[12] * x.dat[7]  * x.dat[10];

  inverse.dat[8] = x.dat[4]  * x.dat[9] * x.dat[15] - 
               x.dat[4]  * x.dat[11] * x.dat[13] - 
               x.dat[8]  * x.dat[5] * x.dat[15] + 
               x.dat[8]  * x.dat[7] * x.dat[13] + 
               x.dat[12] * x.dat[5] * x.dat[11] - 
               x.dat[12] * x.dat[7] * x.dat[9];

  inverse.dat[12] = -x.dat[4]  * x.dat[9] * x.dat[14] + 
                 x.dat[4]  * x.dat[10] * x.dat[13] +
                 x.dat[8]  * x.dat[5] * x.dat[14] - 
                 x.dat[8]  * x.dat[6] * x.dat[13] - 
                 x.dat[12] * x.dat[5] * x.dat[10] + 
                 x.dat[12] * x.dat[6] * x.dat[9];

  inverse.dat[1] = -x.dat[1]  * x.dat[10] * x.dat[15] + 
                x.dat[1]  * x.dat[11] * x.dat[14] + 
                x.dat[9]  * x.dat[2] * x.dat[15] - 
                x.dat[9]  * x.dat[3] * x.dat[14] - 
                x.dat[13] * x.dat[2] * x.dat[11] + 
                x.dat[13] * x.dat[3] * x.dat[10];

  inverse.dat[5] = x.dat[0]  * x.dat[10] * x.dat[15] - 
               x.dat[0]  * x.dat[11] * x.dat[14] - 
               x.dat[8]  * x.dat[2] * x.dat[15] + 
               x.dat[8]  * x.dat[3] * x.dat[14] + 
               x.dat[12] * x.dat[2] * x.dat[11] - 
               x.dat[12] * x.dat[3] * x.dat[10];

  inverse.dat[9] = -x.dat[0]  * x.dat[9] * x.dat[15] + 
                x.dat[0]  * x.dat[11] * x.dat[13] + 
                x.dat[8]  * x.dat[1] * x.dat[15] - 
                x.dat[8]  * x.dat[3] * x.dat[13] - 
                x.dat[12] * x.dat[1] * x.dat[11] + 
                x.dat[12] * x.dat[3] * x.dat[9];

  inverse.dat[13] = x.dat[0]  * x.dat[9] * x.dat[14] - 
                x.dat[0]  * x.dat[10] * x.dat[13] - 
                x.dat[8]  * x.dat[1] * x.dat[14] + 
                x.dat[8]  * x.dat[2] * x.dat[13] + 
                x.dat[12] * x.dat[1] * x.dat[10] - 
                x.dat[12] * x.dat[2] * x.dat[9];

  inverse.dat[2] = x.dat[1]  * x.dat[6] * x.dat[15] - 
               x.dat[1]  * x.dat[7] * x.dat[14] - 
               x.dat[5]  * x.dat[2] * x.dat[15] + 
               x.dat[5]  * x.dat[3] * x.dat[14] + 
               x.dat[13] * x.dat[2] * x.dat[7] - 
               x.dat[13] * x.dat[3] * x.dat[6];

  inverse.dat[6] = -x.dat[0]  * x.dat[6] * x.dat[15] + 
                x.dat[0]  * x.dat[7] * x.dat[14] + 
                x.dat[4]  * x.dat[2] * x.dat[15] - 
                x.dat[4]  * x.dat[3] * x.dat[14] - 
                x.dat[12] * x.dat[2] * x.dat[7] + 
                x.dat[12] * x.dat[3] * x.dat[6];

  inverse.dat[10] = x.dat[0]  * x.dat[5] * x.dat[15] - 
                x.dat[0]  * x.dat[7] * x.dat[13] - 
                x.dat[4]  * x.dat[1] * x.dat[15] + 
                x.dat[4]  * x.dat[3] * x.dat[13] + 
                x.dat[12] * x.dat[1] * x.dat[7] - 
                x.dat[12] * x.dat[3] * x.dat[5];

  inverse.dat[14] = -x.dat[0]  * x.dat[5] * x.dat[14] + 
                 x.dat[0]  * x.dat[6] * x.dat[13] + 
                 x.dat[4]  * x.dat[1] * x.dat[14] - 
                 x.dat[4]  * x.dat[2] * x.dat[13] - 
                 x.dat[12] * x.dat[1] * x.dat[6] + 
                 x.dat[12] * x.dat[2] * x.dat[5];

  inverse.dat[3] = -x.dat[1] * x.dat[6] * x.dat[11] + 
                x.dat[1] * x.dat[7] * x.dat[10] + 
                x.dat[5] * x.dat[2] * x.dat[11] - 
                x.dat[5] * x.dat[3] * x.dat[10] - 
                x.dat[9] * x.dat[2] * x.dat[7] + 
                x.dat[9] * x.dat[3] * x.dat[6];

  inverse.dat[7] = x.dat[0] * x.dat[6] * x.dat[11] - 
               x.dat[0] * x.dat[7] * x.dat[10] - 
               x.dat[4] * x.dat[2] * x.dat[11] + 
               x.dat[4] * x.dat[3] * x.dat[10] + 
               x.dat[8] * x.dat[2] * x.dat[7] - 
               x.dat[8] * x.dat[3] * x.dat[6];

  inverse.dat[11] = -x.dat[0] * x.dat[5] * x.dat[11] + 
                 x.dat[0] * x.dat[7] * x.dat[9] + 
                 x.dat[4] * x.dat[1] * x.dat[11] - 
                 x.dat[4] * x.dat[3] * x.dat[9] - 
                 x.dat[8] * x.dat[1] * x.dat[7] + 
                 x.dat[8] * x.dat[3] * x.dat[5];

  inverse.dat[15] = x.dat[0] * x.dat[5] * x.dat[10] - 
                x.dat[0] * x.dat[6] * x.dat[9] - 
                x.dat[4] * x.dat[1] * x.dat[10] + 
                x.dat[4] * x.dat[2] * x.dat[9] + 
                x.dat[8] * x.dat[1] * x.dat[6] - 
                x.dat[8] * x.dat[2] * x.dat[5];

  F32 det = x.dat[0] * inverse.dat[0] + x.dat[1] * inverse.dat[4] + x.dat[2] * inverse.dat[8] + x.dat[3] * inverse.dat[12];
  B32 result = (det == 0);
  
  if (result) {
    det = 1.0 / det;

    For_U32(it, 16) {
      solved->dat[it] = inverse.dat[it] * det;
    }
  }

  return result;
}

// ------------------------------------------------------------
// #-- CRC32

// NOTE(cmat): Based on https://web.mit.edu/freebsd/head/sys/libkern/crc32.c

var_global U32 CRC32_Lookup_Table[] = {
	0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
	0xe963a535, 0x9e6495a3,	0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
	0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
	0xf3b97148, 0x84be41de,	0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
	0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,	0x14015c4f, 0x63066cd9,
	0xfa0f3d63, 0x8d080df5,	0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
	0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,	0x35b5a8fa, 0x42b2986c,
	0xdbbbc9d6, 0xacbcf940,	0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
	0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
	0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
	0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,	0x76dc4190, 0x01db7106,
	0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
	0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
	0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
	0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
	0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
	0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
	0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
	0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
	0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
	0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
	0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
	0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
	0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
	0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
	0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
	0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
	0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
	0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
	0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
	0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
	0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
	0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
	0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
	0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
	0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
	0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
	0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
	0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
	0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
	0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
	0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
	0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

fn_internal U32 crc32(U64 bytes, U08 *dat) {
  U32 crc = ~0U;
  while (bytes--) {
    crc = CRC32_Lookup_Table[(crc ^ *dat++) & 0xFF] ^ (crc >> 8);
  }

  U32 result = crc ^ ~0U;
  return result;
}

// ------------------------------------------------------------
// #-- Entry Point

fn_internal void core_entry_point(I32 argument_count, char **argument_values) {
  
  // TODO(cmat): Just have a thread_local thread context initialization instead.
  scratch_init_for_thread();

  Array_Str command_line = { };
  base_entry_point(command_line);
}
