// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

// ------------------------------------------------------------
// #-- NOTE(cmat): Link core layer functionality to stb backend
cb_global Arena STBTT_Arena = { };

cb_global void STBTT_backend_init() {
  arena_init(&STBTT_Arena);
}

cb_global void STBTT_backend_free() {
  arena_destroy(&STBTT_Arena);
}

cb_function void *STBTT_malloc_ext(U64 bytes) {
  void *data = arena_push_size(&STBTT_Arena, bytes, .flags = 0);
  return data;
}

cb_function void STBTT_free_ext(void *ptr) {
  // NOTE(cmat): We free memory at STBTT_backend_free all at once.
}

#define stbtt_uint8   U08
#define stbtt_uint16  U16
#define stbtt_uint32  U32

#define stbtt_int8    I08
#define stbtt_int16   I16
#define stbtt_int32   I32

#define STBTT_ifloor(x)  (I32) f32_floor((F32)(x))
#define STBTT_iceil(x)   (I32) f32_ceil ((F32)(x))
#define STBTT_sqrt(x)    f32_sqrt((F32)(x))
#define STBTT_cos(x)     f32_cos((F32)(x))
#define STBTT_acos(x)    f32_acos((F32)(x))
#define STBTT_fabs(x)    f32_fabs((F32)(x))
#define STBTT_pow(x, y)  f32_pow((F32)(x), (F32)(y))
#define STBTT_fmod(x, y) f32_fmod((F32)(x), (F32)(y))

#define STBTT_malloc(x, u) ((void)(u), STBTT_malloc_ext(x))
#define STBTT_free(x, u)   ((void)(u), STBTT_free_ext(x))

#define STBTT_assert(x)   Assert(x, "stb_truetype runtime failure")
#define STBTT_strlen(x)   cstring_len

#define STBTT_memcpy      memory_copy
#define STBTT_memset      memory_fill

#define STB_TRUETYPE_IMPLEMENTATION
#include "thirdparty/stb_truetype.h"
