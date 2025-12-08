// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

// ------------------------------------------------------------
// #-- Stack

#define stack_push(first_, node_) stack_push_ext(first_, node_, next)
#define stack_push_ext(first_, node_, next_name_)   \
  do {                                              \
    (node_)->next_name_ = (first_);                 \
    (first_) = (node_);                             \
  } while(0);

#define stack_pop(first_) stack_pop_ext(first_, next)
#define stack_pop_ext(first_, next_name_)           \
  do {                                              \
    (first_) = (first_)->next_name_;                \
  } while(0);

// ------------------------------------------------------------
// #-- Queue

#define queue_push(first_, last_, node_) queue_push_ext(first_, last_, node_, next)
#define queue_push_ext(first_, last_, node_, next_name_)  \
  do {                                                    \
    if ((first_) == 0) {                                  \
      (first_) = (node_);                                 \
      (last_)  = (node_);                                 \
      (node_)->next_name_ = 0;                            \
    } else {                                              \
      (last_)->next_name_  = (node_);                     \
      (last_)              = (node_);                     \
      (node_)->next_name_  = 0;                           \
    }                                                     \
  } while(0);

#define queue_pop(first_, last_) queue_pop_ext(first_, last_, next)
#define queue_pop_ext(first_, last_, next_name_)          \
  do {                                                    \
    if ((first_) == (last_)) {                            \
      (first_) = 0;                                       \
      (last_)  = 0;                                       \
    } else {                                              \
      (first_) = (first_)->next_name_;                    \
    }                                                     \
  } while(0);

// ------------------------------------------------------------
// #-- Arena

// Arena allocator.
// Arena allocators are implemented using two tricks: MMU, and a doubly linked list
// of reserved chunks. Reserving virtual memory is "free" on most systems
// (you can reserve terabytes of virtual memory without running out),
// we leverage that fact by first reserving large chunks of virtual memory, then comitting as needed.
// If we run out of reserved virtual memory, we allocate a new chunk and add it
// to the linked list.
// -
// Things become tricky when using something like WASM, where there is no
// virtual memory address space (everything is just one, linearly growing
// "physical" address space). The trick in that case is to reserve smaller chunks,
// and treat reserving memory as essentially comitting memory (commit does nothing).
// For now, this is an exception for WASM, and hopefully (someday), the WASM commity
// will wake up from their sleep and actually implement a proper memory model.
// Until then, this is a trick that works.

typedef U32 Arena_Push_Flag;
enum {
  Arena_Push_Flag_Zero_Init = 1 << 0
};

typedef struct { 
  U32             align;
  Arena_Push_Flag flags;
} Arena_Push;

enum {
  Arena_Default_Align = sizeof(void*),
  Arena_Default_Flags = Arena_Push_Flag_Zero_Init,
};

cb_global U64 arena_chunk_magic = u64_pack('A','R','E','N','A','C','H','K');

#if BUILD_DEBUG
typedef struct Arena_Chunk_Header { U64 magic; } Arena_Chunk_Header;
#else
typedef struct Arena_Chunk_Header { } Arena_Chunk_Header;
#endif
 
typedef struct Arena_Chunk {
  Arena_Chunk_Header  header;

  U08                *base_memory;
  U08                *next_page;
  U08                *current;
  U64                 reserved;

  struct Arena_Chunk *prev;
  struct Arena_Chunk *next;
} Arena_Chunk;

typedef U32 Arena_Flag;
enum {
  // NOTE(cmat): Allows growth of arena beyond initial reserved block of memory.
  Arena_Flag_Allow_Chaining = 1 << 0,

  // NOTE(cmat): Always reserve and commit the whole chunk upon creation.
  Arena_Flag_Commit_Whole_Chunk = 1 << 1,

  // NOTE(cmat): If an allocation doesn't fit in the current chunk,
  // before creating a new chunk, backtrack through previous chunks
  // to find one which accepts our allocation.
  // The use-case for this, is for platforms without MMU support (WASM),
  // or where address space is very limited (32bit).
  // Chaining must be enabled for this to have effect!
  Arena_Flag_Backtrack_Before_Chaining = 1 << 2,

  // NOTE(cmat): On WASM, we enable backtracking by default
  // since wasting "virtual" memory is wasting phyisical committed memory.
#if OS_WASM
  Arena_Flag_Defaults = Arena_Flag_Allow_Chaining | Arena_Flag_Backtrack_Before_Chaining
#else
  Arena_Flag_Defaults = Arena_Flag_Allow_Chaining
#endif
};

#if OS_WASM
#define arena_default_chunk_bytes u64_kilobytes(64)
#else
#define arena_default_chunk_bytes u64_megabytes(64)
#endif


typedef struct Arena_Init {
  Arena_Flag flags;
  U64             reserve_initial;
} Arena_Init;

typedef struct Arena {
  Arena_Flag flags; 
  Arena_Chunk    *first_chunk;
  Arena_Chunk    *last_chunk;
} Arena;

cb_function void arena_init_ext (Arena *arena, Arena_Init *init);
cb_function void arena_destroy  (Arena *arena);
cb_function U08 *arena_push_ext (Arena *arena, U64 bytes, Arena_Push *push);
cb_function void arena_clear    (Arena *arena);

#define arena_init(arena, ...)                      arena_init_ext((arena), &(Arena_Init) { .flags = Arena_Flag_Defaults, .reserve_initial = 0, __VA_ARGS__ })
#define arena_push_size(arena, bytes, ...)          arena_push_ext((arena), (bytes), &(Arena_Push) { .align = Arena_Default_Align, .flags = Arena_Default_Flags, __VA_ARGS__ })
#define arena_push_type(arena, type, ...)           (type *)arena_push_size((arena), sizeof(type),  __VA_ARGS__)
#define arena_push_count(arena, type, count, ...)   (type *)arena_push_size((arena), (count) * sizeof(type), __VA_ARGS__)

typedef struct Arena_Temp {
  Arena       *arena;
  Arena_Chunk *rollback_chunk;
  U08         *rollback_current;
  U08         *rollback_page;
} Arena_Temp;

cb_function Arena_Temp arena_temp_start (Arena *arena);
cb_function void       arena_temp_end   (Arena_Temp *temp);

#define Arena_Temp_Scope(arena_, temp_name_) \
  for (Arena_Temp temp_name_ = arena_temp_start(arena_); temp_name_.arena; arena_temp_end(&temp_name_))


// ------------------------------------------------------------
// #-- Scratch Arena

// - Credit goes to Ryan J. Fleury for this concept.
// - The idea is whenever we pass a scratch arena to a function,
// - the function itself might also use the global scratch arena, causing aliasing.
// - This can be referred to as "arena aliasing", similar to pointer aliasing issues
// - well known in C. To fix this, we have two scratch arenas, and we ping pong between the two,
// - so when we ask for a new scrath arena, we're making sure it's not the arena that's been
// - passed to the function.

// TODO(cmat): struct Thread_Context
cb_function void         scratch_init_for_thread (void);
cb_function Arena *      scratch_get_for_thread  (Arena *conflict);

typedef Arena_Temp Scratch;

force_inline cb_function Scratch  scratch_start (Arena *conflict)     { return arena_temp_start(scratch_get_for_thread(conflict)); }
force_inline cb_function void     scratch_end   (Scratch *scratch)    { arena_temp_end(scratch); }

#define Scratch_Scope(scratch_, conflict_) Defer_Scope(*(scratch_) = scratch_start(conflict_), scratch_end(scratch_))
  
// ------------------------------------------------------------
// #-- Array

#define Array_Type(type) struct { U64 len; U64 cap; type *dat; }
typedef struct Array_Header {
  U64 len;
  U64 cap;
} Array_Header;

inline cb_function void array_reserve_ext(Arena *arena, Array_Header *header, void **dat, U64 type_size, U64 reserve_count) {
  Assert(!header->len && !header->cap, "reserving an array that's already in use");
  header->cap = reserve_count;
  *dat        = arena_push_size(arena, type_size * reserve_count);
}

#define array_reserve(arena_, array_, cap_) \
  array_reserve_ext(arena_, (Array_Header *)(array_), (void**)&((array_)->dat), sizeof((array_)->dat[0]), cap_)

#define array_push(array_, value_)                                    \
  do {                                                                \
    Assert((array_)->len != (array_)->cap, "array out of capacity");  \
    ((array_)->dat[(array_)->len++]) = (value_);                      \
  } while(0);

inline cb_function void array_erase_ext(Array_Header *header, U08 *dat, U64 type_bytes, U64 erase_index, U64 erase_count) {
  U64 erase_from_byte = erase_index * type_bytes;
  U64 erase_bytes     = erase_count * type_bytes;
  U64 len_bytes       = header->len * type_bytes;

  U64 copy_byte_idx = erase_from_byte + erase_bytes;
  while (copy_byte_idx < len_bytes) {
    dat[erase_from_byte++] = dat[copy_byte_idx++];
  }

  header->len -= erase_count;
}

#define array_erase(array_, index_, erase_count_) \
  array_erase_ext(                                \
      (Array_Header *)(array_),                   \
      (U08 *)(array_)->dat,                       \
      sizeof((array_)->dat[0]),                   \
      index_,                                     \
      erase_count_);

#define array_clear(array_) ((array_)->len) = 0;
#define array_from_sarray(array_type_, sarray_) \
  (array_type_) { .len = sarray_len(sarray_), .cap = sarray_len(sarray_), .dat = sarray_ }

typedef Array_Type(I08) Array_I08;
typedef Array_Type(I16) Array_I16;
typedef Array_Type(I32) Array_I32;
typedef Array_Type(I64) Array_I64;

typedef Array_Type(U08) Array_U08;
typedef Array_Type(U16) Array_U16;
typedef Array_Type(U32) Array_U32;
typedef Array_Type(U64) Array_U64;

typedef Array_Type(F32) Array_F32;
typedef Array_Type(F64) Array_F64;

typedef Array_Type(Str) Array_Str;

// ------------------------------------------------------------
// #-- Logging

enum {
  Logger_Max_Hooks        = 32,
  Logger_Max_Entry_Length = 1024,
};

typedef U32 Logger_Entry_Type;
enum {
  Logger_Entry_Info,
  Logger_Entry_Debug,
  Logger_Entry_Warning,
  Logger_Entry_Error,
  Logger_Entry_Fatal,

  Logger_Entry_Zone_Start,
  Logger_Entry_Zone_End,
  
  Logger_Entry_Type_Count
};

typedef U32 Logger_Filter_Flag;
enum {
  Logger_Filter_Info     = 1 << 0,
  Logger_Filter_Debug    = 1 << 1,
  Logger_Filter_Warning  = 1 << 2,
  Logger_Filter_Error    = 1 << 3,
  Logger_Filter_Fatal    = 1 << 4,
  Logger_Filter_Zone     = 1 << 5,

  Logger_Filter_Build_Debug =
    Logger_Filter_Debug   |
    Logger_Filter_Info    |
    Logger_Filter_Warning |
    Logger_Filter_Error   |
    Logger_Filter_Fatal   |
    Logger_Filter_Zone,

  Logger_Filter_Build_Release =
    Logger_Filter_Info     |
    Logger_Filter_Warning  |
    Logger_Filter_Error    |
    Logger_Filter_Fatal    |
    Logger_Filter_Zone,

#if BUILD_DEBUG
  Logger_Filter_Build_Active = Logger_Filter_Build_Debug,
#else
  Logger_Filter_Build_Active = Logger_Filter_Build_Release,
#endif
};

force_inline cb_function U32 logger_filter_flag_from_entry_type(U32 entry) {
  U32 result = 0;
  switch (entry) {
    case Logger_Entry_Info:       { result = Logger_Filter_Info; }    break;
    case Logger_Entry_Debug:      { result = Logger_Filter_Debug; }   break;
    case Logger_Entry_Warning:    { result = Logger_Filter_Warning; } break;
    case Logger_Entry_Error:      { result = Logger_Filter_Error; }   break;
    case Logger_Entry_Fatal:      { result = Logger_Filter_Fatal; }   break;
    case Logger_Entry_Zone_Start: { result = Logger_Filter_Zone; }    break;
    case Logger_Entry_Zone_End:   { result = Logger_Filter_Zone; }    break;
    Invalid_Default;
  }

  return result;
}

typedef struct Logger_Entry {
  Logger_Entry_Type type;
  U08               message[Logger_Max_Entry_Length];
  Local_Time        time;
  Function_Metadata meta;
} Logger_Entry;

// NOTE(cmat): Callback prototypes.
typedef void Logger_Write_Entry_Hook  (Logger_Entry_Type type, Str buffer);
typedef void Logger_Format_Entry_Hook (Logger_Entry *entry, U08 *entry_buffer, U32 zone_depth);

// NOTE(cmat): Each thread shares the same global logger.
// - Every log function is thread safe (they are all wrapped in a mutex).
typedef struct Logger_State {
  volatile Logger_Filter_Flag filter;
  U32                         zone_depth;
  U32                         hook_count;
  Logger_Write_Entry_Hook    *write_hooks [Logger_Max_Hooks];
  Logger_Format_Entry_Hook   *format_hooks[Logger_Max_Hooks];
  Mutex                       mutex;
} Logger_State;

cb_function void  logger_set_filter            (Logger_Filter_Flag filter);
cb_function B32   logger_filter_type           (Logger_Entry_Type type);
cb_function void  logger_entry                 (Logger_Entry *entry);
cb_function void  logger_push_hook             (Logger_Write_Entry_Hook *write, Logger_Format_Entry_Hook *format);

cb_function void  logger_write_entry_standard_stream  (Logger_Entry_Type type, Str buffer);
cb_function void  logger_format_entry_minimal         (Logger_Entry *entry, U08 *entry_buffer, U32 zone_depth);
cb_function void  logger_format_entry_detailed        (Logger_Entry *entry, U08 *entry_buffer, U32 zone_depth);

cb_function void  log_message_ext(Logger_Entry_Type type, Function_Metadata func_meta, char *format, ...);

#define log_message(type_, format_, ...) log_message_ext(type_, Function_Metadata_Current, format_,##__VA_ARGS__);
#define log_info(format_, ...)           log_message(Logger_Entry_Info,       format_,##__VA_ARGS__);
#define log_debug(format_, ...)          log_message(Logger_Entry_Debug,      format_,##__VA_ARGS__);
#define log_warning(format_, ...)        log_message(Logger_Entry_Warning,    format_,##__VA_ARGS__);
#define log_fatal(format_, ...)          log_message(Logger_Entry_Fatal,      format_,##__VA_ARGS__);
#define log_zone_start(format_, ...)     log_message(Logger_Entry_Zone_Start, format_,##__VA_ARGS__);
#define log_zone_end()                   log_message(Logger_Entry_Zone_End, "");

#define Log_Zone_Scope(format_, ...)     Defer_Scope(log_message_ext(Logger_Entry_Zone_Start, Function_Metadata_Current, format_,##__VA_ARGS__), \
                                                     log_message_ext(Logger_Entry_Zone_End,   Function_Metadata_Current, ""))

// ------------------------------------------------------------
// #-- Vector Types

typedef union {
  struct { I32 x, y; };
  struct { I32 r, g; };
  struct { I32 u, v; };
  struct { I32 width, height; };  
  I32 dat[2];
} V2_I32;

typedef union {
  struct { U16 x, y; };
  struct { U16 r, g; };
  struct { U16 u, v; };
  struct { U16 width, height; };  
  U16 dat[2];
} V2_U16;

typedef union {
  struct { U32 x, y; };
  struct { U32 r, g; };
  struct { U32 u, v; };
  struct { U32 width, height; };  
  U32 dat[2];
} V2_U32;

typedef union {
  struct { F32 x, y; };
  struct { F32 r, g; };
  struct { F32 u, v; };
  struct { F32 width, height; };  
  F32 dat[2];
} V2_F32;

typedef union {
  struct { F64 x, y; };
  struct { F64 r, g; };
  struct { F64 u, v; };
  struct { F64 width, height; };
  F64 dat[2];
} V2_F64;

typedef union {
  struct { I32 x, y, z; };
  struct { I32 r, g, b; };
  struct { I32 h, s, v; };

  struct { V2_I32 xy; I32 _pad_0; };
  struct { I32 _pad_1; V2_I32 yz; };

  struct { V2_I32 rg; I32 _pad_2; };
  struct { I32 _pad_3; V2_I32 gb; };

  struct { V2_I32 hs; I32 _pad_4; };
  struct { I32 _pad_5; V2_I32 sv; };

  struct { I32 width, height, depth; };
  I32 dat[3];
} V3_I32;

typedef union {
  struct { U16 x, y, z; };
  struct { U16 r, g, b; };
  struct { U16 h, s, v; };

  struct { V2_U16 xy; U16 _pad_0; };
  struct { U16 _pad_1; V2_U16 yz; };

  struct { V2_U16 rg; U16 _pad_2; };
  struct { U16 _pad_3; V2_U16 gb; };

  struct { V2_U16 hs; U16 _pad_4; };
  struct { U16 _pad_5; V2_U16 sv; };

  struct { U16 width, height, depth; };
  U16 dat[3];
} V3_U16;

typedef union {
  struct { U32 x, y, z; };
  struct { U32 r, g, b; };
  struct { U32 h, s, v; };

  struct { V2_U32 xy; U32 _pad_0; };
  struct { U32 _pad_1; V2_U32 yz; };

  struct { V2_U32 rg; U32 _pad_2; };
  struct { U32 _pad_3; V2_U32 gb; };

  struct { V2_U32 hs; U32 _pad_4; };
  struct { U32 _pad_5; V2_U32 sv; };

  struct { U32 width, height, depth; };
  U32 dat[3];
} V3_U32;

typedef union {
  struct { F32 x, y, z; };
  struct { F32 r, g, b; };
  struct { F32 h, s, v; };

  struct { V2_F32 xy; F32 _pad_0; };
  struct { F32 _pad_1; V2_F32 yz; };

  struct { V2_F32 rg; F32 _pad_2; };
  struct { F32 _pad_3; V2_F32 gb; };

  struct { V2_F32 hs; F32 _pad_4; };
  struct { F32 _pad_5; V2_F32 sv; };

  struct { F32 width, height, depth; };
  F32 dat[3];
} V3_F32;

typedef union {
  struct { F64 x, y, z; };
  struct { F64 r, g, b; };
  struct { F64 h, s, v; };

  struct { V2_F64 xy; };
  struct { F64 _pad_1; V2_F64 yz; };

  struct { V2_F64 rg; F64 _pad_2; };
  struct { F64 _pad_3; V2_F64 gb; };

  struct { V2_F64 hs; F64 _pad_4; };
  struct { F64 _pad_5; V2_F64 sv; };

  struct { F64 width, height, depth; };
  F64 dat[3];
} V3_F64;

typedef union {
  struct { I32 x, y, z, w; };
  struct { I32 r, g, b, a; };

  struct { V2_I32 xy; V2_I32 _pad_0; };
  struct { V2_I32 _pad_1; V2_I32 zw; };

  struct { V2_I32 rg; V2_I32 _pad_2; };
  struct { V2_I32 _pad_3; V2_I32 ba; };

  struct { V2_I32 hs; V2_I32 _pad_4; };
  struct { V2_I32 _pad_5; V2_I32 va; };

  struct { I32 _pad_6;  V2_I32 yz; I32 _pad_7; };
  struct { I32 _pad_8;  V2_I32 gb; I32 _pad_9; };
  struct { I32 _pad_10; V2_I32 sv; I32 _pad_11; };

  struct { V3_I32 xyz; I32 _pad_12; };
  struct { V3_I32 rgb; I32 _pad_13; };
  struct { V3_I32 hsv; I32 _pad_14; };

  I32 dat[4];
} V4_I32;

typedef union {
  struct { U16 x, y, z, w; };
  struct { U16 r, g, b, a; };

  struct { V2_U16 xy; V2_U16 _pad_0; };
  struct { V2_U16 _pad_1; V2_U16 zw; };

  struct { V2_U16 rg; V2_U16 _pad_2; };
  struct { V2_U16 _pad_3; V2_U16 ba; };

  struct { V2_U16 hs; V2_U16 _pad_4; };
  struct { V2_U16 _pad_5; V2_U16 va; };

  struct { U16 _pad_6;  V2_U16 yz; U16 _pad_7; };
  struct { U16 _pad_8;  V2_U16 gb; U16 _pad_9; };
  struct { U16 _pad_10; V2_U16 sv; U16 _pad_11; };

  struct { V3_U16 xyz; U16 _pad_12; };
  struct { V3_U16 rgb; U16 _pad_13; };
  struct { V3_U16 hsv; U16 _pad_14; };

  U16 dat[4];
} V4_U16;

typedef union {
  struct { U32 x, y, z, w; };
  struct { U32 r, g, b, a; };

  struct { V2_U32 xy; V2_U32 _pad_0; };
  struct { V2_U32 _pad_1; V2_U32 zw; };

  struct { V2_U32 rg; V2_U32 _pad_2; };
  struct { V2_U32 _pad_3; V2_U32 ba; };

  struct { V2_U32 hs; V2_U32 _pad_4; };
  struct { V2_U32 _pad_5; V2_U32 va; };

  struct { U32 _pad_6;  V2_U32 yz; U32 _pad_7; };
  struct { U32 _pad_8;  V2_U32 gb; U32 _pad_9; };
  struct { U32 _pad_10; V2_U32 sv; U32 _pad_11; };

  struct { V3_U32 xyz; U32 _pad_12; };
  struct { V3_U32 rgb; U32 _pad_13; };
  struct { V3_U32 hsv; U32 _pad_14; };

  U32 dat[4];
} V4_U32;

typedef union {
  struct { F32 x, y, z, w; };
  struct { F32 r, g, b, a; };

  struct { V2_F32 xy; V2_F32 _pad_0; };
  struct { V2_F32 _pad_1; V2_F32 zw; };

  struct { V2_F32 rg; V2_F32 _pad_2; };
  struct { V2_F32 _pad_3; V2_F32 ba; };

  struct { V2_F32 hs; V2_F32 _pad_4; };
  struct { V2_F32 _pad_5; V2_F32 va; };

  struct { F32 _pad_6;  V2_F32 yz; F32 _pad_7; };
  struct { F32 _pad_8;  V2_F32 gb; F32 _pad_9; };
  struct { F32 _pad_10; V2_F32 sv; F32 _pad_11; };

  struct { V3_F32 xyz; F32 _pad_12; };
  struct { V3_F32 rgb; F32 _pad_13; };
  struct { V3_F32 hsv; F32 _pad_14; };

  F32 dat[4];
} V4_F32;

typedef union {
  struct { F64 x, y, z, w; };
  struct { F64 r, g, b, a; };

  struct { V2_F64 xy; V2_F64 _pad_0; };
  struct { V2_F64 _pad_1; V2_F64 zw; };

  struct { V2_F64 rg; V2_F64 _pad_2; };
  struct { V2_F64 _pad_3; V2_F64 ba; };

  struct { V2_F64 hs; V2_F64 _pad_4; };
  struct { V2_F64 _pad_5; V2_F64 va; };

  struct { F64 _pad_6;  V2_F64 yz; F64 _pad_7; };
  struct { F64 _pad_8;  V2_F64 gb; F64 _pad_9; };
  struct { F64 _pad_10; V2_F64 sv; F64 _pad_11; };

  struct { V3_F64 xyz; F64 _pad_12; };
  struct { V3_F64 rgb; F64 _pad_13; };
  struct { V3_F64 hsv; F64 _pad_14; };

  F64 dat[4];
} V4_F64;

typedef V2_F32 V2F;
typedef V3_F32 V3F;
typedef V4_F32 V4F;

typedef V2_I32 V2I;
typedef V3_I32 V3I;
typedef V4_I32 V4I;

typedef V2_U32 V2U;
typedef V3_U32 V3U;
typedef V4_U32 V4U;

Assert_Compiler(sizeof(V2_I32) == 2 * sizeof(I32));
Assert_Compiler(sizeof(V2_U32) == 2 * sizeof(U32));
Assert_Compiler(sizeof(V2_F32) == 2 * sizeof(F32));
Assert_Compiler(sizeof(V2_F64) == 2 * sizeof(F64));

Assert_Compiler(sizeof(V3_I32) == 3 * sizeof(I32));
Assert_Compiler(sizeof(V3_U32) == 3 * sizeof(U32));
Assert_Compiler(sizeof(V3_F32) == 3 * sizeof(F32));
Assert_Compiler(sizeof(V3_F64) == 3 * sizeof(F64));

Assert_Compiler(sizeof(V4_I32) == 4 * sizeof(I32));
Assert_Compiler(sizeof(V4_U32) == 4 * sizeof(U32));
Assert_Compiler(sizeof(V4_F32) == 4 * sizeof(F32));
Assert_Compiler(sizeof(V4_F64) == 4 * sizeof(F64));

#define V2_Expand(v) ((v).x), ((v).y)
#define V3_Expand(v) ((v).x), ((v).y), ((v).z)
#define V4_Expand(v) ((v).x), ((v).y), ((v).z), ((v).w)

typedef Array_Type(V2F) Array_V2F;
typedef Array_Type(V3F) Array_V3F;
typedef Array_Type(V4F) Array_V4F;

typedef Array_Type(V2I) Array_V2I;
typedef Array_Type(V3I) Array_V3I;
typedef Array_Type(V4I) Array_V4I;

typedef Array_Type(V2U) Array_V2U;
typedef Array_Type(V3U) Array_V3U;
typedef Array_Type(V4U) Array_V4U;

typedef Array_Type(V2_U16) Array_V2_U16;
typedef Array_Type(V3_U16) Array_V3_U16;
typedef Array_Type(V4_U16) Array_V4_U16;

// ------------------------------------------------------------
// #-- Vector Ops

force_inline cb_function V2F v2f       (F32 x, F32 y)                  { return (V2F) { x, y };       }
force_inline cb_function V3F v3f       (F32 x, F32 y, F32 z)           { return (V3F) { x, y, z };    }
force_inline cb_function V4F v4f       (F32 x, F32 y, F32 z, F32 w)    { return (V4F) { x, y, z, w }; }

force_inline cb_function V2I v2i       (I32 x, I32 y)                  { return (V2I) { x, y };       }
force_inline cb_function V3I v3i       (I32 x, I32 y, I32 z)           { return (V3I) { x, y, z };    }
force_inline cb_function V4I v4i       (I32 x, I32 y, I32 z, I32 w)    { return (V4I) { x, y, z, w }; }

force_inline cb_function V2U v2u       (U32 x, U32 y)                  { return (V2U) { x, y };       }
force_inline cb_function V3U v3u       (U32 x, U32 y, U32 z)           { return (V3U) { x, y, z };    }
force_inline cb_function V4U v4u       (U32 x, U32 y, U32 z, U32 w)    { return (V4U) { x, y, z, w }; }

force_inline cb_function V2_U16 v2_u16 (U16 x, U16 y)                  { return (V2_U16) { x, y };       }
force_inline cb_function V3_U16 v3_u16 (U16 x, U16 y, U16 z)           { return (V3_U16) { x, y, z };    }
force_inline cb_function V4_U16 v4_u16 (U16 x, U16 y, U16 z, U16 w)    { return (V4_U16) { x, y, z, w }; }

force_inline cb_function V2F v2f_f32   (F32 x) { return (V2F) { x, x };       }
force_inline cb_function V3F v3f_f32   (F32 x) { return (V3F) { x, x, x };    }
force_inline cb_function V4F v4f_f32   (F32 x) { return (V4F) { x, x, x, x }; }

force_inline cb_function V2I v2i_s     (I32 x) { return (V2I) { x, x };       }
force_inline cb_function V3I v3i_s     (I32 x) { return (V3I) { x, x, x };    }
force_inline cb_function V4I v4i_s     (I32 x) { return (V4I) { x, x, x, x }; }

// NOTE(cmat): 2D
inline cb_function V2F v2f_add           (V2F lhs, V2F rhs)         { return (V2F) { lhs.x + rhs.x, lhs.y + rhs.y }; }
inline cb_function V2F v2f_sub           (V2F lhs, V2F rhs)         { return (V2F) { lhs.x - rhs.x, lhs.y - rhs.y }; }
inline cb_function V2F v2f_had           (V2F lhs, V2F rhs)         { return (V2F) { lhs.x * rhs.x, lhs.y * rhs.y }; }
inline cb_function V2F v2f_mul           (F32 lhs, V2F rhs)         { return (V2F) { lhs * rhs.x, lhs * rhs.y };     }
inline cb_function V2F v2f_div           (V2F lhs, F32 rhs)         { return (V2F) { lhs.x / rhs, lhs.y / rhs };     }
inline cb_function V2F v2f_add_f32       (V2F lhs, F32 rhs)         { return (V2F) { lhs.x + rhs, lhs.y + rhs };     }
inline cb_function V2F v2f_sub_f32       (V2F lhs, F32 rhs)         { return (V2F) { lhs.x - rhs, lhs.y - rhs };     }
inline cb_function V2F v2f_mul_f32       (V2F lhs, F32 rhs)         { return (V2F) { lhs.x * rhs, lhs.y * rhs };     }

inline cb_function V2I v2i_add            (V2I lhs, V2I rhs)         { return (V2I) { lhs.x + rhs.x, lhs.y + rhs.y }; }
inline cb_function V2I v2i_sub            (V2I lhs, V2I rhs)         { return (V2I) { lhs.x - rhs.x, lhs.y - rhs.y }; }
inline cb_function V2I v2i_had            (V2I lhs, V2I rhs)         { return (V2I) { lhs.x * rhs.x, lhs.y * rhs.y }; }
inline cb_function V2I v2i_mul            (I32 lhs, V2I rhs)         { return (V2I) { lhs * rhs.x, lhs * rhs.y };     }
inline cb_function V2I v2i_div            (V2I lhs, I32 rhs)         { return (V2I) { lhs.x / rhs, lhs.y / rhs };     }
inline cb_function V2I v2i_add_i32        (V2I lhs, I32 rhs)         { return (V2I) { lhs.x + rhs, lhs.y + rhs };     }
inline cb_function V2I v2i_sub_i32        (V2I lhs, I32 rhs)         { return (V2I) { lhs.x - rhs, lhs.y - rhs };     }
inline cb_function V2I v2i_mul_i32        (V2I lhs, I32 rhs)         { return (V2I) { lhs.x * rhs, lhs.y * rhs };     }

inline cb_function V2U v2u_add            (V2U lhs, V2U rhs)         { return (V2U) { lhs.x + rhs.x, lhs.y + rhs.y }; }
inline cb_function V2U v2u_sub            (V2U lhs, V2U rhs)         { return (V2U) { lhs.x - rhs.x, lhs.y - rhs.y }; }
inline cb_function V2U v2u_had            (V2U lhs, V2U rhs)         { return (V2U) { lhs.x * rhs.x, lhs.y * rhs.y }; }
inline cb_function V2U v2u_mul            (U32 lhs, V2U rhs)         { return (V2U) { lhs * rhs.x, lhs * rhs.y };     }
inline cb_function V2U v2u_div            (V2U lhs, U32 rhs)         { return (V2U) { lhs.x / rhs, lhs.y / rhs };     }
inline cb_function V2U v2u_add_u32        (V2U lhs, U32 rhs)         { return (V2U) { lhs.x + rhs, lhs.y + rhs };     }
inline cb_function V2U v2u_sub_u32        (V2U lhs, U32 rhs)         { return (V2U) { lhs.x - rhs, lhs.y - rhs };     }
inline cb_function V2U v2u_mul_u32        (V2U lhs, U32 rhs)         { return (V2U) { lhs.x * rhs, lhs.y * rhs };     }

inline cb_function V2_U16 v2_u16_add      (V2_U16 lhs, V2_U16 rhs)   { return (V2_U16) { lhs.x + rhs.x, lhs.y + rhs.y }; }
inline cb_function V2_U16 v2_u16_sub      (V2_U16 lhs, V2_U16 rhs)   { return (V2_U16) { lhs.x - rhs.x, lhs.y - rhs.y }; }
inline cb_function V2_U16 v2_u16_had      (V2_U16 lhs, V2_U16 rhs)   { return (V2_U16) { lhs.x * rhs.x, lhs.y * rhs.y }; }
inline cb_function V2_U16 v2_u16_mul      (U16    lhs, V2_U16 rhs)   { return (V2_U16) { lhs * rhs.x, lhs * rhs.y };     }
inline cb_function V2_U16 v2_u16_div      (V2_U16 lhs, U16    rhs)   { return (V2_U16) { lhs.x / rhs, lhs.y / rhs };     }
inline cb_function V2_U16 v2_u16_add_u16  (V2_U16 lhs, U16    rhs)   { return (V2_U16) { lhs.x + rhs, lhs.y + rhs };     }
inline cb_function V2_U16 v2_u16_sub_u16  (V2_U16 lhs, U16    rhs)   { return (V2_U16) { lhs.x - rhs, lhs.y - rhs };     }
inline cb_function V2_U16 v2_u16_mul_u16  (V2_U16 lhs, U16    rhs)   { return (V2_U16) { lhs.x * rhs, lhs.y * rhs };     }

// NOTE(cmat): 3D
inline cb_function V3F v3f_add         (V3F lhs, V3F rhs)      { return (V3F) { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };  }
inline cb_function V3F v3f_sub         (V3F lhs, V3F rhs)      { return (V3F) { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };  }
inline cb_function V3F v3f_mul         (F32 lhs, V3F rhs)      { return (V3F) { lhs * rhs.x, lhs * rhs.y, lhs * rhs.z };        }
inline cb_function V3F v3f_had         (V3F lhs, V3F rhs)      { return (V3F) { lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z };  }
inline cb_function V3F v3f_div         (V3F lhs, F32 rhs)      { return (V3F) { lhs.x / rhs, lhs.y / rhs, lhs.z / rhs };        }
inline cb_function V3F v3f_add_f32     (V3F lhs, F32 rhs)      { return (V3F) { lhs.x + rhs, lhs.y + rhs, lhs.z + rhs };        }
inline cb_function V3F v3f_sub_f32     (V3F lhs, F32 rhs)      { return (V3F) { lhs.x - rhs, lhs.y - rhs, lhs.z - rhs };        }
inline cb_function V3F v3f_mul_f32     (V3F lhs, F32 rhs)      { return (V3F) { lhs.x * rhs, lhs.y * rhs, lhs.z * rhs };        }

inline cb_function V3I v3i_add         (V3I lhs, V3I rhs)      { return (V3I) { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};   }
inline cb_function V3I v3i_sub         (V3I lhs, V3I rhs)      { return (V3I) { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};   }
inline cb_function V3I v3i_had         (V3I lhs, V3I rhs)      { return (V3I) { lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z };  }
inline cb_function V3I v3i_mul         (I32 lhs, V3I rhs)      { return (V3I) { lhs * rhs.x, lhs * rhs.y, lhs * rhs.z };        }
inline cb_function V3I v3i_div         (V3I lhs, I32 rhs)      { return (V3I) { lhs.x / rhs, lhs.y / rhs, lhs.z / rhs };        }
inline cb_function V3I v3i_add_i32     (V3I lhs, I32 rhs)      { return (V3I) { lhs.x + rhs, lhs.y + rhs, lhs.z + rhs };        }
inline cb_function V3I v3i_sub_i32     (V3I lhs, I32 rhs)      { return (V3I) { lhs.x - rhs, lhs.y - rhs, lhs.z - rhs };        }
inline cb_function V3I v3i_mul_i32     (V3I lhs, I32 rhs)      { return (V3I) { lhs.x * rhs, lhs.y * rhs, lhs.z * rhs };        }

inline cb_function V3U v3u_add         (V3U lhs, V3U rhs)      { return (V3U) { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};   }
inline cb_function V3U v3u_sub         (V3U lhs, V3U rhs)      { return (V3U) { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};   }
inline cb_function V3U v3u_had         (V3U lhs, V3U rhs)      { return (V3U) { lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z };  }
inline cb_function V3U v3u_mul         (U32 lhs, V3U rhs)      { return (V3U) { lhs * rhs.x, lhs * rhs.y, lhs * rhs.z };        }
inline cb_function V3U v3u_div         (V3U lhs, U32 rhs)      { return (V3U) { lhs.x / rhs, lhs.y / rhs, lhs.z / rhs };        }
inline cb_function V3U v3u_add_u32     (V3U lhs, U32 rhs)      { return (V3U) { lhs.x + rhs, lhs.y + rhs, lhs.z + rhs };        }
inline cb_function V3U v3u_sub_u32     (V3U lhs, U32 rhs)      { return (V3U) { lhs.x - rhs, lhs.y - rhs, lhs.z - rhs };        }
inline cb_function V3U v3u_mul_u32     (V3U lhs, U32 rhs)      { return (V3U) { lhs.x * rhs, lhs.y * rhs, lhs.z * rhs };        }

inline cb_function V3_U16 v3_u16_add         (V3_U16 lhs, V3_U16 rhs)      { return (V3_U16) { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};   }
inline cb_function V3_U16 v3_u16_sub         (V3_U16 lhs, V3_U16 rhs)      { return (V3_U16) { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};   }
inline cb_function V3_U16 v3_u16_had         (V3_U16 lhs, V3_U16 rhs)      { return (V3_U16) { lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z };  }
inline cb_function V3_U16 v3_u16_mul         (U16 lhs, V3_U16 rhs)         { return (V3_U16) { lhs * rhs.x, lhs * rhs.y, lhs * rhs.z };        }
inline cb_function V3_U16 v3_u16_div         (V3_U16 lhs, U16 rhs)         { return (V3_U16) { lhs.x / rhs, lhs.y / rhs, lhs.z / rhs };        }
inline cb_function V3_U16 v3_u16_add_u16     (V3_U16 lhs, U16 rhs)         { return (V3_U16) { lhs.x + rhs, lhs.y + rhs, lhs.z + rhs };        }
inline cb_function V3_U16 v3_u16_sub_u16     (V3_U16 lhs, U16 rhs)         { return (V3_U16) { lhs.x - rhs, lhs.y - rhs, lhs.z - rhs };        }
inline cb_function V3_U16 v3_u16_mul_u16     (V3_U16 lhs, U16 rhs)         { return (V3_U16) { lhs.x * rhs, lhs.y * rhs, lhs.z * rhs };        }


// NOTE(cmat): 4D
inline cb_function V4F v4f_add         (V4F lhs, V4F rhs)      { return (V4F) { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w }; }
inline cb_function V4F v4f_sub         (V4F lhs, V4F rhs)      { return (V4F) { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w};  }
inline cb_function V4F v4f_had         (V4F lhs, V4F rhs)      { return (V4F) { lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z, lhs.w * rhs.w};  }
inline cb_function V4F v4f_mul         (F32 lhs, V4F rhs)      { return (V4F) { lhs * rhs.x, lhs * rhs.y, lhs * rhs.z, lhs * rhs.w };         }
inline cb_function V4F v4f_div         (V4F lhs, F32 rhs)      { return (V4F) { lhs.x / rhs, lhs.y / rhs, lhs.z / rhs, lhs.w / rhs };         }
inline cb_function V4F v4f_add_f32     (V4F lhs, F32 rhs)      { return (V4F) { lhs.x + rhs, lhs.y + rhs, lhs.z + rhs, lhs.w + rhs };         }
inline cb_function V4F v4f_sub_f32     (V4F lhs, F32 rhs)      { return (V4F) { lhs.x - rhs, lhs.y - rhs, lhs.z - rhs, lhs.w - rhs };         }
inline cb_function V4F v4f_mul_f32     (V4F lhs, F32 rhs)      { return (V4F) { lhs.x * rhs, lhs.y * rhs, lhs.z * rhs, lhs.w * rhs };         }

inline cb_function V4I v4i_add         (V4I lhs, V4I rhs)      { return (V4I) { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w }; }
inline cb_function V4I v4i_sub         (V4I lhs, V4I rhs)      { return (V4I) { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w};  }
inline cb_function V4I v4i_had         (V4I lhs, V4I rhs)      { return (V4I) { lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z, lhs.w * rhs.w};  }
inline cb_function V4I v4i_mul         (I32 lhs, V4I rhs)      { return (V4I) { lhs * rhs.x, lhs * rhs.y, lhs * rhs.z, lhs * rhs.w };         }
inline cb_function V4I v4i_div         (V4I lhs, I32 rhs)      { return (V4I) { lhs.x / rhs, lhs.y / rhs, lhs.z / rhs, lhs.w / rhs };         }
inline cb_function V4I v4i_add_i32     (V4I lhs, I32 rhs)      { return (V4I) { lhs.x + rhs, lhs.y + rhs, lhs.z + rhs, lhs.w + rhs };         }
inline cb_function V4I v4i_sub_i32     (V4I lhs, I32 rhs)      { return (V4I) { lhs.x - rhs, lhs.y - rhs, lhs.z - rhs, lhs.w - rhs };         }
inline cb_function V4I v4i_mul_i32     (V4I lhs, I32 rhs)      { return (V4I) { lhs.x * rhs, lhs.y * rhs, lhs.z * rhs, lhs.w * rhs };         }

inline cb_function V4U v4u_add         (V4U lhs, V4U rhs)      { return (V4U) { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w }; }
inline cb_function V4U v4u_sub         (V4U lhs, V4U rhs)      { return (V4U) { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w};  }
inline cb_function V4U v4u_had         (V4U lhs, V4U rhs)      { return (V4U) { lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z, lhs.w * rhs.w};  }
inline cb_function V4U v4u_mul         (U32 lhs, V4U rhs)      { return (V4U) { lhs * rhs.x, lhs * rhs.y, lhs * rhs.z, lhs * rhs.w };         }
inline cb_function V4U v4u_div         (V4U lhs, U32 rhs)      { return (V4U) { lhs.x / rhs, lhs.y / rhs, lhs.z / rhs, lhs.w / rhs };         }
inline cb_function V4U v4u_add_u32     (V4U lhs, U32 rhs)      { return (V4U) { lhs.x + rhs, lhs.y + rhs, lhs.z + rhs, lhs.w + rhs };         }
inline cb_function V4U v4u_sub_u32     (V4U lhs, U32 rhs)      { return (V4U) { lhs.x - rhs, lhs.y - rhs, lhs.z - rhs, lhs.w - rhs };         }
inline cb_function V4U v4u_mul_u32     (V4U lhs, U32 rhs)      { return (V4U) { lhs.x * rhs, lhs.y * rhs, lhs.z * rhs, lhs.w * rhs };         }

inline cb_function V4_U16 v4_u16_add         (V4_U16 lhs, V4_U16 rhs)   { return (V4_U16) { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w }; }
inline cb_function V4_U16 v4_u16_sub         (V4_U16 lhs, V4_U16 rhs)   { return (V4_U16) { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w};  }
inline cb_function V4_U16 v4_u16_had         (V4_U16 lhs, V4_U16 rhs)   { return (V4_U16) { lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z, lhs.w * rhs.w};  }
inline cb_function V4_U16 v4_u16_mul         (U16 lhs, V4_U16 rhs)      { return (V4_U16) { lhs * rhs.x, lhs * rhs.y, lhs * rhs.z, lhs * rhs.w };         }
inline cb_function V4_U16 v4_u16_div         (V4_U16 lhs, U16 rhs)      { return (V4_U16) { lhs.x / rhs, lhs.y / rhs, lhs.z / rhs, lhs.w / rhs };         }
inline cb_function V4_U16 v4_u16_add_u32     (V4_U16 lhs, U16 rhs)      { return (V4_U16) { lhs.x + rhs, lhs.y + rhs, lhs.z + rhs, lhs.w + rhs };         }
inline cb_function V4_U16 v4_u16_sub_u32     (V4_U16 lhs, U16 rhs)      { return (V4_U16) { lhs.x - rhs, lhs.y - rhs, lhs.z - rhs, lhs.w - rhs };         }
inline cb_function V4_U16 v4_u16_mul_u32     (V4_U16 lhs, U16 rhs)      { return (V4_U16) { lhs.x * rhs, lhs.y * rhs, lhs.z * rhs, lhs.w * rhs };         }

// NOTE(cmat): Floating Point Specific.

#define NOZ_Epsilon 1.0e-6f

inline cb_function F32 v2f_dot          (V2F lhs, V2F rhs)     { return lhs.x * rhs.x + lhs.y * rhs.y;                                 }
inline cb_function F32 v3f_dot          (V3F lhs, V3F rhs)     { return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;                 }
inline cb_function F32 v4f_dot          (V4F lhs, V4F rhs)     { return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z + lhs.w * rhs.w; }

inline cb_function F32 v2f_len2         (V2F x)                { return v2f_dot(x, x); }
inline cb_function F32 v3f_len2         (V3F x)                { return v3f_dot(x, x); }
inline cb_function F32 v4f_len2         (V4F x)                { return v4f_dot(x, x); }

inline cb_function F32 v2f_len          (V2F x)                { return f32_sqrt(v2f_len2(x)); }
inline cb_function F32 v3f_len          (V3F x)                { return f32_sqrt(v3f_len2(x)); }
inline cb_function F32 v4f_len          (V4F x)                { return f32_sqrt(v4f_len2(x)); }

inline cb_function F32 v2f_dist         (V2F lhs, V2F rhs)     { return v2f_len(v2f_sub(lhs, rhs)); }
inline cb_function F32 v3f_dist         (V3F lhs, V3F rhs)     { return v3f_len(v3f_sub(lhs, rhs)); }
inline cb_function F32 v4f_dist         (V4F lhs, V4F rhs)     { return v4f_len(v4f_sub(lhs, rhs)); }

inline cb_function V2F v2f_nor          (V2F x)                { return v2f_div(x, v2f_len(x)); }
inline cb_function V3F v3f_nor          (V3F x)                { return v3f_div(x, v3f_len(x)); }
inline cb_function V4F v4f_nor          (V4F x)                { return v4f_div(x, v4f_len(x)); }

inline cb_function V2F v2f_noz          (V2F x)                { F32 len = v2f_len(x); return len <= NOZ_Epsilon ? v2f_f32(0.f) : v2f_div(x, len); }
inline cb_function V3F v3f_noz          (V3F x)                { F32 len = v3f_len(x); return len <= NOZ_Epsilon ? v3f_f32(0.f) : v3f_div(x, len); }
inline cb_function V4F v4f_noz          (V4F x)                { F32 len = v4f_len(x); return len <= NOZ_Epsilon ? v4f_f32(0.f) : v4f_div(x, len); }

inline cb_function V2F v2f_abs          (V2F x)                { return v2f(f32_abs(x.x), f32_abs(x.y));                             }
inline cb_function V3F v3f_abs          (V3F x)                { return v3f(f32_abs(x.x), f32_abs(x.y), f32_abs(x.z));               }
inline cb_function V4F v4f_abs          (V4F x)                { return v4f(f32_abs(x.x), f32_abs(x.y), f32_abs(x.z), f32_abs(x.w)); }

inline cb_function V2F v2f_saturate     (V2F x)                { return v2f(f32_clamp(x.x, 0.f, 1.f), f32_clamp(x.y, 0.f, 1.f));                                                     }
inline cb_function V3F v3f_saturate     (V3F x)                { return v3f(f32_clamp(x.x, 0.f, 1.f), f32_clamp(x.y, 0.f, 1.f), f32_clamp(x.z, 0.f, 1.f));                           }
inline cb_function V4F v4f_saturate     (V4F x)                { return v4f(f32_clamp(x.x, 0.f, 1.f), f32_clamp(x.y, 0.f, 1.f), f32_clamp(x.z, 0.f, 1.f), f32_clamp(x.w, 0.f, 1.f)); }

inline cb_function V2F v2f_frac         (V2F x)                { return v2f(f32_fract(x.x), f32_fract(x.y));                                 }
inline cb_function V3F v3f_frac         (V3F x)                { return v3f(f32_fract(x.x), f32_fract(x.y), f32_fract(x.z));                 }
inline cb_function V4F v4f_frac         (V4F x)                { return v4f(f32_fract(x.x), f32_fract(x.y), f32_fract(x.z), f32_fract(x.w)); }

inline cb_function B32 v2f_all          (V2F x)                { return x.x != 0.f && x.y != 0.f;                             }
inline cb_function B32 v3f_all          (V3F x)                { return x.x != 0.f && x.y != 0.f && x.z != 0.f;               }
inline cb_function B32 v4f_all          (V4F x)                { return x.x != 0.f && x.y != 0.f && x.z != 0.f && x.w != 0.f; }

inline cb_function B32 v2f_any          (V2F x)                { return x.x != 0.f || x.y != 0.f;                             }
inline cb_function B32 v3f_any          (V3F x)                { return x.x != 0.f || x.y != 0.f || x.z != 0.f;               }
inline cb_function B32 v4f_any          (V4F x)                { return x.x != 0.f || x.y != 0.f || x.z != 0.f || x.w != 0.f; }

inline cb_function V2F v2f_reflect      (V2F in, V2F normal)   { return v2f_sub(in, v2f_mul( v2f_dot(in, normal), v2f_mul(2.f, normal))); }
inline cb_function V3F v3f_reflect      (V3F in, V3F normal)   { return v3f_sub(in, v3f_mul( v3f_dot(in, normal), v3f_mul(2.f, normal))); }
inline cb_function V4F v4f_reflect      (V4F in, V4F normal)   { return v4f_sub(in, v4f_mul( v4f_dot(in, normal), v4f_mul(2.f, normal))); }

inline cb_function V2F v2f_rcp          (V2F x)                { return v2f(1.f / x.x, 1.f / x.y);                       }
inline cb_function V3F v3f_rcp          (V3F x)                { return v3f(1.f / x.x, 1.f / x.y, 1.f / x.z);            }
inline cb_function V4F v4f_rcp          (V4F x)                { return v4f(1.f / x.x, 1.f / x.y, 1.f / x.z, 1.f / x.w); }

// NOTE(cmat): Cross only works in 3 and 5 dimensions.
inline cb_function V3F v3f_cross(V3F lhs, V3F rhs) {
  return (V3F) {
    .x = lhs.y * rhs.z - lhs.z * rhs.y,
    .y = lhs.z * rhs.x - lhs.x * rhs.z,
    .z = lhs.x * rhs.y - lhs.y * rhs.x
  };
}

// ------------------------------------------------------------
// #-- Matrix Types

typedef union {
  struct { V2_I32 row_1, row_2; };
  struct { I32  e11, e12,
                e21, e22; };
  I32 ele[2][2];
  I32 dat[2 * 2];
} M2_I32;

typedef union {
  struct { V2_F32 row_1, row_2; };
  struct { F32  e11, e12,
                e21, e22; };
  F32 ele[2][2];
  F32 dat[2 * 2];
} M2_F32;

typedef union {
  struct { V2_F64 row_1, row_2; };
  struct { F64  e11, e12,
                e21, e22; };
  F64 ele[2][2];
  F64 dat[2 * 2];
} M2_F64;

typedef union {
  struct { V3_I32 row_1, row_2, row_3; };
  struct { I32  e11, e12, e13,
                e21, e22, e23,
                e31, e32, e33; };
  I32 ele[3][3];
  U32 dat[3 * 3];
} M3_I32;

typedef union {
  struct { V3_F32 row_1, row_2, row_3; };
  struct { F32  e11, e12, e13,
                e21, e22, e23,
                e31, e32, e33; };
  F32 ele[3][3];
  F32 dat[3 * 3];
} M3_F32;

typedef union {
  struct { V3_F64 row_1, row_2, row_3; };
  struct { F64  e11, e12, e13,
                e21, e22, e23,
                e31, e32, e33; };
  F64 ele[3][3];
  F64 dat[3 * 3];
} M3_F64;

typedef union {
  struct { V4_I32 row_1, row_2, row_3, row_4; };
  struct { I32  e11, e12, e13, e14,
                e21, e22, e23, e24,
                e31, e32, e33, e34,
                e41, e42, e43, e44; };
  I32 ele[4][4];
  I32 dat[4 * 4];
} M4_I32;

typedef union {
  struct { V4_F32 row_1, row_2, row_3, row_4; };

  struct { F32  e11, e12, e13, e14,
                e21, e22, e23, e24,
                e31, e32, e33, e34,
                e41, e42, e43, e44; };
  F32 ele[4][4];
  F32 dat[4 * 4];
} M4_F32;

typedef union {
  struct { V4_F64 row_1, row_2, row_3, row_4; };

  struct { F64  e11, e12, e13, e14,
                e21, e22, e23, e24,
                e31, e32, e33, e34,
                e41, e42, e43, e44; };
  F64 ele[4][4];
  F64 dat[4 * 4];
} M4_F64;

typedef M2_I32 M2I;
typedef M3_I32 M3I;
typedef M4_I32 M4I;

typedef M2_F32 M2F;
typedef M3_F32 M3F;
typedef M4_F32 M4F;

typedef Array_Type(M2I) M2I_Array;
typedef Array_Type(M3I) M3I_Array;
typedef Array_Type(M4I) M4I_Array;

typedef Array_Type(M2F) M2F_Array;
typedef Array_Type(M3F) M3F_Array;
typedef Array_Type(M4F) M4F_Array;

Assert_Compiler(sizeof(M2_I32) == 2 * 2 * sizeof(I32));
Assert_Compiler(sizeof(M2_F32) == 2 * 2 * sizeof(F32));
Assert_Compiler(sizeof(M2_F64) == 2 * 2 * sizeof(F64));

Assert_Compiler(sizeof(M3_I32) == 3 * 3 * sizeof(I32));
Assert_Compiler(sizeof(M3_F32) == 3 * 3 * sizeof(F32));
Assert_Compiler(sizeof(M3_F64) == 3 * 3 * sizeof(F64));

Assert_Compiler(sizeof(M4_I32) == 4 * 4 * sizeof(I32));
Assert_Compiler(sizeof(M4_F32) == 4 * 4 * sizeof(F32));
Assert_Compiler(sizeof(M4_F64) == 4 * 4 * sizeof(F64));

// ------------------------------------------------------------
// #-- Region Types

#pragma pack(push, 1)

typedef union Region2_I32 {
  struct { I32 x0, y0, x1, y1; };
  struct { V2I min, max;       };
} Region2_I32;

typedef union Region2_F32 {
  struct { F32 x0, y0, x1, y1; };
  struct { V2F min, max;       };
} Region2_F32;

typedef union Region2_F64 {
  struct { F64 x0, y0, x1, y1; };
  struct { V2F min, max;       };
} Region2_F64;

typedef union Region3_I32 {
  struct { I32 x0, y0, z0, x1, y1, z1;  };
  struct { V3I min, max;                };
} Region3_I32;

typedef union Region3_F32 {
  struct { F32 x0, y0, z0, x1, y1, z1;  };
  struct { V3F min, max;                };
} Region3_F32;

typedef union Region3_F64 {
  struct { F64 x0, y0, z0, x1, y1, z1;  };
  struct { V3F min, max;                };
} Region3_F64;

#pragma pack(pop)

Assert_Compiler(sizeof(Region2_I32) == 4 * sizeof(I32));
Assert_Compiler(sizeof(Region2_F32) == 4 * sizeof(F32));
Assert_Compiler(sizeof(Region2_F64) == 4 * sizeof(F64));

Assert_Compiler(sizeof(Region3_I32) == 6 * sizeof(I32));
Assert_Compiler(sizeof(Region3_F32) == 6 * sizeof(F32));
Assert_Compiler(sizeof(Region3_F64) == 6 * sizeof(F64));

typedef Region2_I32 R2_I32;
typedef Region2_F32 R2_F32;
typedef Region2_F64 R2_F64;

typedef Region3_I32 R3_I32;
typedef Region3_F32 R3_F32;
typedef Region3_F64 R3_F64;

typedef R2_I32 R2I;
typedef R2_F32 R2F;

typedef R3_I32 R3I;
typedef R3_F32 R3F;

typedef Array_Type(R2I) Array_R2I;
typedef Array_Type(R2F) Array_R2F;

typedef Array_Type(R3I) Array_R3I;
typedef Array_Type(R3F) Array_R3F;

// ------------------------------------------------------------
// #-- Region Ops

force_inline cb_function R2I r2i    (I32 x0, I32 y0, I32 x1, I32 y1) { return (R2I) { .x0 = x0, .y0 = y0, .x1 = x1, .y1 = y1, }; }
force_inline cb_function R2F r2f    (F32 x0, F32 y0, F32 x1, F32 y1) { return (R2F) { .x0 = x0, .y0 = y0, .x1 = x1, .y1 = y1, }; }
force_inline cb_function R2I r2i_v  (V2I min, V2I max)               { return (R2I) { .min = min, .max = max };                  }
force_inline cb_function R2F r2f_v  (V2F min, V2F max)               { return (R2F) { .min = min, .max = max };                  }

force_inline cb_function R3I r3i    (I32 x0, I32 y0, I32 z0, I32 x1, I32 y1, I32 z1)  { return (R3I) { .x0 = x0, .y0 = y0, .z0 = z0, .x1 = x1, .y1 = y1, .z1 = z1, }; }
force_inline cb_function R3F r3f    (F32 x0, F32 y0, F32 z0, F32 x1, F32 y1, F32 z1)  { return (R3F) { .x0 = x0, .y0 = y0, .z0 = z0, .x1 = x1, .y1 = y1, .z1 = z1, }; }
force_inline cb_function R3I r3i_v  (V3I min, V3I max)                                { return (R3I) { .min = min, .max = max };                                      }
force_inline cb_function R3F r3f_v  (V3F min, V3F max)                                { return (R3F) { .min = min, .max = max };                                      }

// ------------------------------------------------------------
// #-- Matrix Ops

inline cb_function M2F m2f_id(void) {
  M2F result;
  zero_fill(&result);
  result.e11 = 1;
  result.e22 = 1;
  return result;
}

inline cb_function M3F m3f_id(void) {
  M3F result;
  zero_fill(&result);
  result.e11 = 1;
  result.e22 = 1;
  result.e33 = 1;
  return result;
}

inline cb_function M4F m4f_id(void) {
  M4F result;
  zero_fill(&result);
  result.e11 = 1;
  result.e22 = 1;
  result.e33 = 1;
  result.e44 = 1;
  return result;
}

M2F m2f_mul(M2F lhs, M2F rhs) {
  M2F result;
  zero_fill(&result);

  For_U32(row, 2) {
    For_U32(col, 2) {
      result.ele[row][col] = lhs.ele[row][0] * rhs.ele[0][col] +
                             lhs.ele[row][1] * rhs.ele[1][col];
    }
  }

  return result;
}

M3F m3f_mul(M3F lhs, M3F rhs) {
  M3F result;
  zero_fill(&result);

  For_U32(row, 3) {
    For_U32(col, 3) {
      result.ele[row][col] = lhs.ele[row][0] * rhs.ele[0][col] +
                             lhs.ele[row][1] * rhs.ele[1][col] +
                             lhs.ele[row][2] * rhs.ele[2][col];
    }
  }

  return result;
}

M4F m4f_mul(M4F lhs, M4F rhs) {
  M4F result;
  zero_fill(&result);

  For_U32(row, 4) {
    For_U32(col, 4) {
      result.ele[row][col] = lhs.ele[row][0] * rhs.ele[0][col] +
                             lhs.ele[row][1] * rhs.ele[1][col] +
                             lhs.ele[row][2] * rhs.ele[2][col] +
                             lhs.ele[row][3] * rhs.ele[3][col];
    }
  }

  return result;
}

V4F m4f_mul_v4f(M4F lhs, V4F rhs) {
  V4F result;
  zero_fill(&result);

  For_U32(row, 4) {
    result.dat[row] = lhs.ele[row][0] * rhs.dat[0] +
                      lhs.ele[row][1] * rhs.dat[1] +
                      lhs.ele[row][2] * rhs.dat[2] +
                      lhs.ele[row][3] * rhs.dat[3];
  }

  return result;
}

inline cb_function M2F m2f_trans(M2F x) {
  M2F result = {};
  zero_fill(&result);

  For_U32(col, 2) {
    For_U32(row, 2) {
      result.ele[col][row] = x.ele[row][col];
    }
  }

  return result;
}

inline cb_function M3F m3f_trans(M3F x) {
  M3F result = {};
  zero_fill(&result);

  For_U32(col, 3) {
    For_U32(row, 3) {
      result.ele[col][row] = x.ele[row][col];
    }
  }

  return result;
}

inline cb_function M4F m4f_trans(M4F x) {
  M4F result = {};
  zero_fill(&result);

  For_U32(col, 4) {
    For_U32(row, 4) {
      result.ele[col][row] = x.ele[row][col];
    }
  }

  return result;
}

inline cb_function M2F m2f_add(M2F lhs, M2F rhs) {
  M2F result = { };
  For_U32(it, 2*2) {
    result.dat[it] = lhs.dat[it] + rhs.dat[it];
  }

  return result;
}

inline cb_function M3F m3f_add(M3F lhs, M3F rhs) {
  M3F result = { };
  For_U32(it, 3*3) {
    result.dat[it] = lhs.dat[it] + rhs.dat[it];
  }

  return result;
}

inline cb_function M4F m4f_add(M4F lhs, M4F rhs) {
  M4F result = { };
  For_U32(it, 4*4) {
    result.dat[it] = lhs.dat[it] + rhs.dat[it];
  }

  return result;
}

inline cb_function M2F m2f_sub(M2F lhs, M2F rhs) {
  M2F result = { };
  For_U32(it, 2*2) {
    result.dat[it] = lhs.dat[it] - rhs.dat[it];
  }

  return result;
}

inline cb_function M3F m3f_sub(M3F lhs, M3F rhs) {
  M3F result = { };
  For_U32(it, 3*3) {
    result.dat[it] = lhs.dat[it] - rhs.dat[it];
  }

  return result;
}

inline cb_function M4F m4f_sub(M4F lhs, M4F rhs) {
  M4F result = { };
  For_U32(it, 4*4) {
    result.dat[it] = lhs.dat[it] - rhs.dat[it];
  }

  return result;
}

inline cb_function M2F m2f_had(M2F lhs, M2F rhs) {
  M2F result = { };
  For_U32(it, 2*2) {
    result.dat[it] = lhs.dat[it] * rhs.dat[it];
  }

  return result;
}

inline cb_function M3F m3f_had(M3F lhs, M3F rhs) {
  M3F result = { };
  For_U32(it, 3*3) {
    result.dat[it] = lhs.dat[it] * rhs.dat[it];
  }

  return result;
}

inline cb_function M4F m4f_had(M4F lhs, M4F rhs) {
  M4F result = { };
  For_U32(it, 4*4) {
    result.dat[it] = lhs.dat[it] * rhs.dat[it];
  }

  return result;
}

inline cb_function M2F m2f_mul_f32(F32 lhs, M2F rhs) {
  M2F result = { };
  For_U32(it, 2*2) {
    result.dat[it] = lhs * rhs.dat[it];
  }

  return result;
}

inline cb_function M3F m3f_mul_f32(F32 lhs, M3F rhs) {
  M3F result = { };
  For_U32(it, 3*3) {
    result.dat[it] = lhs * rhs.dat[it];
  }

  return result;
}

inline cb_function M4F m4f_mul_f32(F32 lhs, M4F rhs) {
  M4F result = { };
  For_U32(it, 4*4) {
    result.dat[it] = lhs * rhs.dat[it];
  }

  return result;
}

inline cb_function M2F m2f_div_f32(M2F lhs, F32 rhs) {
  M2F result = { };
  For_U32(it, 2*2) {
    result.dat[it] = lhs.dat[it] / rhs;
  }

  return result;
}

inline cb_function M3F m3f_div_f32(M3F lhs, F32 rhs) {
  M3F result = { };
  For_U32(it, 3*3) {
    result.dat[it] = lhs.dat[it] / rhs;
  }

  return result;
}

inline cb_function M4F m4f_div_f32(M4F lhs, F32 rhs) {
  M4F result = { };
  For_U32(it, 4*4) {
    result.dat[it] = lhs.dat[it] / rhs;
  }

  return result;
}

inline cb_function F32 m2f_trace(M2F x) { return x.e11 + x.e22; }
inline cb_function F32 m3f_trace(M3F x) { return x.e11 + x.e22 + x.e33; }
inline cb_function F32 m4f_trace(M4F x) { return x.e11 + x.e22 + x.e33 + x.e44; }

cb_function F32 m2f_det(M2F x);
cb_function F32 m3f_det(M3F x);
cb_function F32 m4f_det(M4F x);
cb_function B32 m4f_inv(M4F x, M4F *solved);

// ------------------------------------------------------------
// #-- Color Spaces

typedef V3F RGB_F32;
typedef V3F HSV_F32;
typedef V4F RGBA_F32;
typedef V4F HSVA_F32;
typedef U32 RGBA_U32;
typedef U32 HSVA_U32;

typedef RGB_F32 RGB;
typedef HSV_F32 HSV;

typedef RGBA_F32 RGBA;
typedef HSVA_F32 HSVA;

cb_function RGB       rgb_from_hsv       (HSV rgb);
cb_function HSV       hsv_from_rgb       (RGB hsv);
cb_function RGBA      rgba_from_hsva     (HSVA rgb);
cb_function HSVA      hsva_from_rgba     (RGBA hsv);
cb_function RGBA_U32  rgba_u32_from_rgba (RGBA_F32 rgba);
cb_function RGBA_U32  abgr_u32_from_rgba (RGBA_F32 rgba);

// ------------------------------------------------------------
// #-- Interpolation

inline cb_function F32 f32_lerp  (F32 t, F32 a, F32 b)  { return (1.f - t) * a + b;                    }
inline cb_function V2F v2f_lerp  (F32 t, V2F a, V2F b)  { return v2f_add(v2f_mul((1.f - t), a), b);    }
inline cb_function V3F v3f_lerp  (F32 t, V3F a, V3F b)  { return v3f_add(v3f_mul((1.f - t), a), b);    }
inline cb_function V4F v4f_lerp  (F32 t, V4F a, V4F b)  { return v4f_add(v4f_mul((1.f - t), a), b);    }

#define rgb_lerp  v3f_lerp
#define rgba_lerp v4f_lerp
#define hsv_lerp  v3f_lerp
#define hsva_lerp v4f_lerp

inline cb_function F32 f32_smoothstep (F32 t, F32 a, F32 b) {
  t = f32_clamp((t - a) / (b - a), 0, 1);
  return t * t * (3.f - 2.f * t);
}

inline cb_function V2F v2f_smoothstep (F32 t, V2F a, V2F b) {
  return v2f(f32_smoothstep(t, a.x, b.x), f32_smoothstep(t, a.y, b.y));
}

inline cb_function V3F v3f_smoothstep (F32 t, V3F a, V3F b) {
  return v3f(f32_smoothstep(t, a.x, b.x), f32_smoothstep(t, a.y, b.y), f32_smoothstep(t, a.z, b.z));
}

inline cb_function V4F v4f_smoothstep (F32 t, V4F a, V4F b) {
  return v4f(f32_smoothstep(t, a.x, b.x), f32_smoothstep(t, a.y, b.y), f32_smoothstep(t, a.z, b.z), f32_smoothstep(t, a.w, b.w));
}

// ------------------------------------------------------------
// #-- Interpolation

cb_function V2F v2f_spline_catmull(F32 t, V2F p1, V2F p2, V2F p3, V2F p4);
cb_function V3F v3f_spline_catmull(F32 t, V3F p1, V3F p2, V3F p3, V3F p4);
cb_function V4F v4f_spline_catmull(F32 t, V4F p1, V4F p2, V4F p3, V4F p4);

cb_function V2F v2f_spline_catmull_dt(F32 t, V2F p1, V2F p2, V2F p3, V2F p4);
cb_function V3F v3f_spline_catmull_dt(F32 t, V3F p1, V3F p2, V3F p3, V3F p4);
cb_function V4F v4f_spline_catmull_dt(F32 t, V4F p1, V4F p2, V4F p3, V4F p4);

// ------------------------------------------------------------
// #-- Pseudo-Random Number Generation

typedef U64 Random_Seed;

// NOTE(cmat): XORSHIFT implementation.
inline cb_function U64 random_next (Random_Seed *seed) {
  U64 x = *seed;
  x ^= x << 13;
  x ^= x >> 7;
  x ^= x << 17;

  *seed = x;
  return x;
}

// NOTE(cmat): Random values in the range: [min, max].
inline cb_function U64 i64_random            (Random_Seed *seed, I64 min, I64 max) { return min + (I64)(random_next(seed) % (U64)(max - min + 1)); }
inline cb_function U64 u64_random            (Random_Seed *seed, U64 min, U64 max) { return min + (U64)(random_next(seed) % (U64)(max - min + 1)); }
 
// NOTE(cmat): Random unilateral values (between [0, 1]).
inline cb_function F32 f32_random_unilateral (Random_Seed *seed) { return (F32)random_next(seed) / (F32)u64_limit_max;                                                                               }
inline cb_function F64 f64_random_unilateral (Random_Seed *seed) { return (F64)random_next(seed) / (F64)u64_limit_max;                                                                               }
inline cb_function V2F v2f_random_unilateral (Random_Seed *seed) { return v2f(f32_random_unilateral(seed), f32_random_unilateral(seed));                                                             }
inline cb_function V3F v3f_random_unilateral (Random_Seed *seed) { return v3f(f32_random_unilateral(seed), f32_random_unilateral(seed),  f32_random_unilateral(seed));                               }
inline cb_function V4F v4f_random_unilateral (Random_Seed *seed) { return v4f(f32_random_unilateral(seed), f32_random_unilateral(seed),  f32_random_unilateral(seed), f32_random_unilateral(seed));  }

// NOTE(cmat): Random bilateral values (between [-1, 1]).
inline cb_function F32 f32_random_bilateral  (Random_Seed *seed) { return 2.f * f32_random_unilateral(seed) - 1.f;                                                                                   }
inline cb_function F64 f64_random_bilateral  (Random_Seed *seed) { return 2.  * f64_random_unilateral(seed) - 1.;                                                                                    }
inline cb_function V2F v2f_random_bilateral  (Random_Seed *seed) { return v2f(f32_random_bilateral (seed), f32_random_bilateral (seed));                                                             }
inline cb_function V3F v3f_random_bilateral  (Random_Seed *seed) { return v3f(f32_random_bilateral (seed), f32_random_bilateral (seed),  f32_random_bilateral (seed));                               }
inline cb_function V4F v4f_random_bilateral  (Random_Seed *seed) { return v4f(f32_random_bilateral (seed), f32_random_bilateral (seed),  f32_random_bilateral (seed),  f32_random_bilateral(seed));  }

// ------------------------------------------------------------
// #-- SIMD

#if ARCH_ARM
#include <arm_neon.h>

// NOTE(cmat): Basic 4x wide types

typedef union {
  float32x4_t simd;
  F32      data[4];
} F32_X04;

typedef struct {
  uint32x4_t simd;
} Mask_X04;

// NOTE(cmat): Basic 4x wide ops
force_inline F32_X04  f32_x04_load                        (F32 *ptr)                                { return (F32_X04)                              { .simd = vld1q_f32(ptr) }; }
force_inline F32_X04  f32_x04_load_f32                    (F32 x)                                   { F32 ptr[4] = { x, x, x, x }; return (F32_X04) { .simd = vld1q_f32(ptr) }; }

force_inline F32_X04  f32_x04_add                         (F32_X04 lhs, F32_X04 rhs)                { return (F32_X04)  { .simd = vaddq_f32(lhs.simd, rhs.simd) }; }
force_inline F32_X04  f32_x04_sub                         (F32_X04 lhs, F32_X04 rhs)                { return (F32_X04)  { .simd = vsubq_f32(lhs.simd, rhs.simd) }; }
force_inline F32_X04  f32_x04_mul                         (F32_X04 lhs, F32_X04 rhs)                { return (F32_X04)  { .simd = vmulq_f32(lhs.simd, rhs.simd) }; }
force_inline F32_X04  f32_x04_div                         (F32_X04 lhs, F32_X04 rhs)                { return (F32_X04)  { .simd = vdivq_f32(lhs.simd, rhs.simd) }; }
force_inline F32_X04  f32_x04_square_root                 (F32_X04 x)                               { return (F32_X04)  { .simd = vsqrtq_f32(x.simd) }; }
force_inline F32_X04  f32_x04_fused_mul_add               (F32_X04 a, F32_X04 b, F32_X04 c)         { return (F32_X04)  { .simd = vfmaq_f32(c.simd, b.simd, a.simd) }; }
force_inline F32_X04  f32_x04_fused_mul_sub               (F32_X04 a, F32_X04 b, F32_X04 c)         { return (F32_X04)  { .simd = vfmsq_f32(c.simd, b.simd, a.simd) }; }
force_inline Mask_X04 f32_x04_mask_greater_than_or_equal  (F32_X04 lhs, F32_X04 rhs)                { return (Mask_X04) { .simd = vcgeq_f32(lhs.simd, rhs.simd) }; }
force_inline F32_X04  f32_x04_blend                       (F32_X04 a, F32_X04 b, Mask_X04 mask)     { return (F32_X04)  { .simd = vbslq_f32(mask.simd, a.simd, b.simd) }; }

#elif ARCH_X86
#include <immintrin.h>

// NOTE(cmat): Basic 4x wide types
typedef union {
  __m128 simd;
  F32 data[4];
} F32_X04;

typedef struct {
  __mmask8 simd;
} Mask_X04;

// NOTE(cmat): Basic 4x wide ops
force_inline F32_X04  f32_x04_load                        (F32 *ptr)                                { return (F32_X04)  { .simd = _mm_load_ps(ptr) }; }
force_inline F32_X04  f32_x04_load_f32                    (F32 x)                                   { return (F32_X04)  { .simd = _mm_set1_ps(x) }; }

force_inline F32_X04  f32_x04_add                         (F32_X04 lhs, F32_X04 rhs)                { return (F32_X04)  { .simd = _mm_add_ps(lhs.simd, rhs.simd) }; }
force_inline F32_X04  f32_x04_sub                         (F32_X04 lhs, F32_X04 rhs)                { return (F32_X04)  { .simd = _mm_sub_ps(lhs.simd, rhs.simd) }; }
force_inline F32_X04  f32_x04_mul                         (F32_X04 lhs, F32_X04 rhs)                { return (F32_X04)  { .simd = _mm_mul_ps(lhs.simd, rhs.simd) }; }
force_inline F32_X04  f32_x04_div                         (F32_X04 lhs, F32_X04 rhs)                { return (F32_X04)  { .simd = _mm_div_ps(lhs.simd, rhs.simd) }; }
force_inline F32_X04  f32_x04_square_root                 (F32_X04 x)                               { return (F32_X04)  { .simd = _mm_sqrt_ps(x.simd) }; }
force_inline F32_X04  f32_x04_fused_mul_add               (F32_X04 a, F32_X04 b, F32_X04 c)         { return (F32_X04)  { .simd = _mm_fmadd_ps(a.simd, b.simd, c.simd) }; }
force_inline F32_X04  f32_x04_fused_mul_sub               (F32_X04 a, F32_X04 b, F32_X04 c)         { return (F32_X04)  { .simd = _mm_fmsub_ps(a.simd, b.simd, c.simd) }; }
force_inline Mask_X04 f32_x04_mask_greater_than_or_equal  (F32_X04 lhs, F32_X04 rhs)                { return (Mask_X04) { .simd = _mm_cmp_ps_mask(lhs.simd, rhs.simd, _CMP_GE_OQ) }; }
force_inline F32_X04  f32_x04_blend                       (F32_X04 a, F32_X04 b, Mask_X04 mask)     { return (F32_X04)  { .simd = _mm_mask_blend_ps(mask.simd, a.simd, b.simd) }; }

#endif

// ------------------------------------------------------------
// #-- Entry Point

cb_function void base_entry_point(Array_Str command_line);

