// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

// ------------------------------------------------------------
// #-- NOTE(cmat): Unspported in WASM backend.

#define WASM_Not_Supported(proc_) core_panic(str_lit(Macro_Stringize(proc_) ": not supported on WASM backend."));

cb_function B32       core_directory_create (Str folder_path)                                     { WASM_Not_Supported(core_directory_create); return 0;            }
cb_function B32       core_directory_delete (Str folder_path)                                     { WASM_Not_Supported(core_directory_delete); return 0;            }
cb_function Core_File core_file_open        (Str file_path, Core_File_Access_Flag flags)          { WASM_Not_Supported(core_file_open); return (Core_File) { };     }
cb_function U64       core_file_size        (Core_File *file)                                     { WASM_Not_Supported(core_file_size); return 0; return 0;         }
cb_function void      core_file_write       (Core_File *file, U64 offset, U64 bytes, void *data)  { WASM_Not_Supported(core_file_write);                            }
cb_function void      core_file_read        (Core_File *file, U64 offset, U64 bytes, void *data)  { WASM_Not_Supported(core_file_read);                             }
cb_function void      core_file_close       (Core_File *file)                                     { WASM_Not_Supported(core_file_close);                            }

// ------------------------------------------------------------
// #-- JS - WASM core API.
extern void js_core_stream_write  (U32 stream_mode, U32 string_len, char *string_txt);
extern F64  js_core_unix_time     (void);
extern void js_core_panic         (U32 string_len, char *string_txt);

cb_global Core_Context wasm_context = { };
cb_function Core_Context *core_context(void) {
  return &wasm_context;
}

cb_function void core_stream_write(Str buffer, Core_Stream stream) {
  U32 stream_mode = 1;
  switch (stream) {
    case Core_Stream_Standard_Output: { stream_mode = 1; } break;
    case Core_Stream_Standard_Error:  { stream_mode = 2; } break;
    Invalid_Default;
  }
  
  js_core_stream_write(stream_mode, (U32)buffer.len, (char *)buffer.txt);
}

cb_function void core_panic(Str reason) {
  js_core_panic((U32)reason.len, (char *)reason.txt);
}

cb_function Local_Time core_local_time(void) {
  Local_Time result     = { };
  U64 time_since_epoch  = (U64)js_core_unix_time();
  U64 unix_seconds      = time_since_epoch / 1000;
  U64 unix_microseconds = (time_since_epoch % 1000) * 1000;
  Local_Time local_time = local_time_from_unix_time(unix_seconds, unix_microseconds);

  return local_time;
}

// TODO(cmat): Implement our custom WASM allocator, instead of relying on 'walloc.c'
void *malloc(__SIZE_TYPE__ size);
void  free  (void *ptr);

// NOTE(cmat): MMU.
// - Unfortunately, WASM does *not* support virtual memory,
// - so we have to assume each reserve is a commit.
// - This means the end-user can't allocate huge chunks of virtual memory,
// - since those are committed immediately.
// - Hopefully this changes in the future, and WASM introduces proper memory
// - managment primitives, but given how things have been going with the standard...
// - not holding my breath.

cb_function U08 *core_memory_reserve(U64 bytes) {
  U08 *result = (U08 *)malloc(bytes);
  return result;
}

cb_function void core_memory_unreserve  (void *virtual_base, U64 bytes) {
  // TODO(cmat): This ignores bytes, which is technically fine for the current arena allocator,
  // - but is definitely not fine otherwise.
  // - Maybe the correct solution is to actually just store bytes in any allocation upfront in a header,
  // - and only allow a chunk of virtual memory to be freed all at once.
  free(virtual_base);
}

cb_function void core_memory_commit   (void *virtual_base, U64 bytes, Core_Commit_Flag mode)  { }
cb_function void core_memory_uncommit (void *virtual_base, U64 bytes)                         { }


// ------------------------------------------------------------
// #-- WASM entry point.

__attribute__((export_name("wasm_entry_point")))
void wasm_entry_point(U32 cpu_logical_cores) {
  wasm_context.cpu_name           = str_lit("WASM VM");
  wasm_context.cpu_logical_cores  = cpu_logical_cores;
  wasm_context.mmu_page_bytes     = u64_kilobytes(64);
  wasm_context.ram_capacity_bytes = u64_gigabytes(4);

  core_entry_point(0, 0);
}


