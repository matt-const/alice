// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

// ------------------------------------------------------------
// #-- NOTE(cmat): Unspported in WASM backend.

#define WASM_Not_Supported(proc_) co_panic(str_lit(Macro_Stringize(proc_) ": not supported on WASM backend."));

fn_internal B32       co_directory_create (Str folder_path)                                     { WASM_Not_Supported(co_directory_create); return 0;            }
fn_internal B32       co_directory_delete (Str folder_path)                                     { WASM_Not_Supported(co_directory_delete); return 0;            }
fn_internal CO_File co_file_open        (Str file_path, CO_File_Access_Flag flags)          { WASM_Not_Supported(co_file_open); return (CO_File) { };     }
fn_internal U64       co_file_size        (CO_File *file)                                     { WASM_Not_Supported(co_file_size); return 0; return 0;         }
fn_internal void      co_file_write       (CO_File *file, U64 offset, U64 bytes, void *data)  { WASM_Not_Supported(co_file_write);                            }
fn_internal void      co_file_read        (CO_File *file, U64 offset, U64 bytes, void *data)  { WASM_Not_Supported(co_file_read);                             }
fn_internal void      co_file_close       (CO_File *file)                                     { WASM_Not_Supported(co_file_close);                            }

// ------------------------------------------------------------
// #-- JS - WASM core API.
fn_external void js_co_stream_write  (U32 stream_mode, U32 string_len, char *string_txt);
fn_external F64  js_co_unix_time     (void);
fn_external void js_co_panic         (U32 string_len, char *string_txt);

var_global CO_Context wasm_context = { };
fn_internal CO_Context *co_context(void) {
  return &wasm_context;
}

fn_internal void co_stream_write(Str buffer, CO_Stream stream) {
  U32 stream_mode = 1;
  switch (stream) {
    case CO_Stream_Standard_Output: { stream_mode = 1; } break;
    case CO_Stream_Standard_Error:  { stream_mode = 2; } break;
    Invalid_Default;
  }
  
  js_co_stream_write(stream_mode, (U32)buffer.len, (char *)buffer.txt);
}

fn_internal void co_panic(Str reason) {
  js_co_panic((U32)reason.len, (char *)reason.txt);
}

fn_internal Local_Time co_local_time(void) {
  Local_Time result     = { };
  U64 time_since_epoch  = (U64)js_co_unix_time();
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

fn_internal U08 *co_memory_reserve(U64 bytes) {
  U08 *result = (U08 *)malloc(bytes);
  return result;
}

fn_internal void co_memory_unreserve  (void *virtual_base, U64 bytes) {
  // TODO(cmat): This ignores bytes, which is technically fine for the current arena allocator,
  // - but is definitely not fine otherwise.
  // - Maybe the correct solution is to actually just store bytes in any allocation upfront in a header,
  // - and only allow a chunk of virtual memory to be freed all at once.
  free(virtual_base);
}

fn_internal void co_memory_commit   (void *virtual_base, U64 bytes, CO_Commit_Flag mode)  { }
fn_internal void co_memory_uncommit (void *virtual_base, U64 bytes)                         { }

// ------------------------------------------------------------
// #-- WASM entry point.

__attribute__((export_name("wasm_entry_point")))
fn_entry void wasm_entry_point(U32 cpu_logical_cores) {
  wasm_context.cpu_name           = str_lit("WASM VM");
  wasm_context.cpu_logical_cores  = cpu_logical_cores;
  wasm_context.mmu_page_bytes     = u64_kilobytes(64);
  wasm_context.ram_capacity_bytes = u64_gigabytes(4);

  co_entry_point(0, 0);
}


