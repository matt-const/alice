// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

// ------------------------------------------------------------
// #-- Codebase Markup

#define cb_function       static
#define cb_global         static
#define cb_local_persist  static

// ------------------------------------------------------------
// #-- Build Flags

#if !defined(BUILD_DEBUG)
# warning "BUILD_DEBUG not defined"
# define BUILD_DEBUG 0
#endif

#if !defined(BUILD_ASSERT)
# error "BUILD_ASSERT not defined"
# define BUILD_ASSERT 0
#endif

// ------------------------------------------------------------
// #-- Platform Detection

#define COMPILER_MSVC   0
#define COMPILER_CLANG  0
#define COMPILER_GCC    0

#if defined(_MSC_VER)
# undef  COMPILER_MSVC
# define COMPILER_MSVC 1
#elif defined(__clang__)
# undef  COMPILER_CLANG
# define COMPILER_CLANG 1
#elif defined(__GNUC__) 
# undef  COMPILER_GCC
# define COMPILER_GCC 1
#else
# error "unsupported compiler"
#endif

#define OS_WIN32 0
#define OS_MACOS 0
#define OS_LINUX 0
#define OS_WASM  0

#if defined(__wasm__)
# undef OS_WASM
# define OS_WASM 1
#elif defined(_WIN32) || defined(WIN32)
# undef  OS_WIN32
# define OS_WIN32 1
#elif defined(__APPLE__)
# undef  OS_MACOS
# define OS_MACOS 1
#elif defined(__linux__)
# undef  OS_LINUX
# define OS_LINUX 1
#else
# error "unsupported operating system"
#endif

#define ARCH_X86  0
#define ARCH_ARM  0
#define ARCH_WASM 0

#if COMPILER_MSVC
# if defined(__x86_64__)
#  undef  ARCH_X86
#   define ARCH_X86 1
# elif defined(_M_ARM)
#   undef  ARCH_ARM
#   define ARCH_ARM 1
# elif defined(__wasm__)
#   undef  ARCH_WASM
#   define ARCH_WASM 1
# else
#   error "unsupported architecture"
# endif
#elif COMPILER_CLANG || COMPILER_GCC
# if defined(__x86_64__)
#   undef  ARCH_X86
#   define ARCH_X86 1
# elif __ARM_ARCH_ISA_A64
#   undef ARCH_ARM
#   define ARCH_ARM 1
# elif defined(__wasm__)
#   undef ARCH_WASM
#   define ARCH_WASM 1
# else
#   error "unsupported architecture"
# endif
#endif

// ------------------------------------------------------------
// #-- Compiler Warnings

#if COMPILER_CLANG
#pragma clang diagnostic ignored "-Winitializer-overrides"
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wformat-invalid-specifier"
#endif

// ------------------------------------------------------------
// #-- Macro Utilities

#define Macro_Stringize_2(x) #x
#define Macro_Stringize_1(x) Macro_Stringize_2(x)
#define Macro_Stringize(x)   Macro_Stringize_1(x)

#define Macro_Join_2(x, y) x ## y
#define Macro_Join_1(x, y) Macro_Join_2(x, y)
#define Macro_Join(x, y)   Macro_Join_1(x, y)

#define Macro_Counter __COUNTER



// ------------------------------------------------------------
// #-- Compile-Time Assertion

#define Assert_Compiler(condition) _Static_assert(condition, Macro_Stringize(condition))

// ------------------------------------------------------------
// #-- Branch Prediction

#if COMPILER_MSVC
# define If_Likely(x)     if(!!(x))
# define If_Unlikely(x)   if(!!(x))
#elif COMPILER_CLANG || COMPILER_GCC
# define If_Likely(x)     if(__builtin_expect(!!(x), 1))
# define If_Unlikely(x)   if(__builtin_expect(!!(x), 1))
#endif

// ------------------------------------------------------------
// #-- Clang Address Sanitizer

#define ASAN_ENABLED 0
#define asan_poison_region(address, size)   (void)(address), (void)(size)
#define asan_unpoison_region(address, size) (void)(address), (void)(size)

#if COMPILER_CLANG
# if __has_feature(address_sanitizer) || defined(__SANITIZE_ADDRESS__)

void __asan_poison_memory_region    (void const volatile *addr, __SIZE_TYPE__ size);
void __asan_unpoison_memory_region  (void const volatile *addr, __SIZE_TYPE__ size);

#   undef ASAN_ENABLED
#   undef asan_poison_region
#   undef asan_unpoison_region
#   define ASAN_ENABLED 1
#   define asan_poison_region(address, size)   __asan_poison_memory_region(address, size)
#   define asan_unpoison_region(address, size) __asan_unpoison_memory_region(address, size)
# endif
#endif

// ------------------------------------------------------------
// #-- Primitive Types

#if COMPILER_GCC || COMPILER_CLANG

typedef __INT8_TYPE__   I08;
typedef __INT16_TYPE__  I16;
typedef __INT32_TYPE__  I32;
typedef __INT64_TYPE__  I64;
typedef __UINT8_TYPE__  U08;
typedef __UINT16_TYPE__ U16;
typedef __UINT32_TYPE__ U32;
typedef __UINT64_TYPE__ U64;

#elif COMPILER_MSVC
typedef __int8   I08;
typedef __int16  I16;
typedef __int32  I32;
typedef __int64  I64;
typedef __uint8  U08;
typedef __uint16 U16;
typedef __uint32 U32;
typedef __uint64 U64;

#endif

typedef float  F32;
typedef double F64;

Assert_Compiler(sizeof(I08) == 1);
Assert_Compiler(sizeof(I16) == 2);
Assert_Compiler(sizeof(I32) == 4);
Assert_Compiler(sizeof(I64) == 8);

Assert_Compiler(sizeof(U08) == 1);
Assert_Compiler(sizeof(U16) == 2);
Assert_Compiler(sizeof(U32) == 4);
Assert_Compiler(sizeof(U64) == 8);

Assert_Compiler(sizeof(F32) == 4);
Assert_Compiler(sizeof(F64) == 8);

typedef I08 B08;
typedef I16 B16;
typedef I32 B32;
typedef I64 B64;

// ------------------------------------------------------------
// #-- Keywords

#if COMPILER_MSVC
# define thread_local __declspec(thread)
#elif COMPILER_CLANG || COMPILER_GCC
# define thread_local __thread
#endif

#if COMPILER_MSVC
# define force_inline inline __forceinline
#elif COMPILER_CLANG || COMPILER_GCC
# define force_inline inline __attribute__((always_inline))
#endif

#define alignas(x_)              _Alignas(x_)
#define offsetof(type_, member_) ((U64)&(((type_ *)0)->member_))

// ------------------------------------------------------------
// #-- Strings

typedef struct Str {
  U64  len;
  I08 *txt;
} Str;

#define str_lit(literal_) ((Str) { .txt = (I08 *)(literal_), .len = sizeof(literal_) - 1 })
#define str_expand(str_) ((U32)(str_.len)), (str_.txt)

force_inline cb_function Str str(U64 len, I08 *txt) { return (Str) { .len = len, .txt =txt }; }

// ------------------------------------------------------------
// #-- Strings Base Operations

cb_function Str str_slice                 (Str base, U64 start, U64 len);
cb_function Str str_from_cstr             (char *cstring);
cb_function Str str_trim                  (Str string);
cb_function B32 str_equals                (Str lhs, Str rhs);
cb_function B32 str_starts_with           (Str base, Str start);
cb_function B32 str_contains              (Str base, Str sub);
cb_function B32 str_equals_any_case       (Str lhs, Str rhs);
cb_function B32 str_starts_with_any_case  (Str base, Str start);
cb_function B32 str_contains_any_case     (Str base, Str sub);
cb_function U64 str_hash                  (Str string);

cb_function I64 i64_from_str              (Str string);
cb_function F64 f64_from_str              (Str string);
cb_function B32 b32_from_str              (Str string);

// ------------------------------------------------------------
// #-- Meta-Data Collection

typedef struct {
  Str function;
  Str filename;
  U32 line;
} Function_Metadata;


#define Function_Metadata_Current ((Function_Metadata) {          \
    .function     = str_lit(__func__),                            \
    .filename     = str_lit(__FILE__),                            \
    .line         = __LINE__                                      \
})

// ------------------------------------------------------------
// #-- Control Flow

#define For_I32(it_, max_)              for (I32 it_ = 0; (it_) < (max_); ++(it_))
#define For_U32(it_, max_)              for (U32 it_ = 0; (it_) < (max_); ++(it_))

#define For_I32_Range(it_, min_, max_)  for (I32 it_ = (min_); (it_) < (max_); ++(it_))
#define For_U32_Range(it_, min_, max_)  for (U32 it_ = (min_); (it_) < (max_); ++(it_))

#define For_I32_Reverse(it_, max_)      for (I32 it_ = (max_ - 1); (it_) != 0; ++(it_))
#define For_U32_Reverse(it_, max_)      for (U32 it_ = (max_ - 1); (it_) != 0; ++(it_))

#define For_I64(it_, max_)              for (I64 it_ = 0; (it_) < (max_); ++(it_))
#define For_U64(it_, max_)              for (U64 it_ = 0; (it_) < (max_); ++(it_))

#define For_I64_Range(it_, min_, max_)  for (I64 it_ = (min_); (it_) < (max_); ++(it_))
#define For_U64_Range(it_, min_, max_)  for (U64 it_ = (min_); (it_) < (max_); ++(it_))

#define For_I64_Reverse(it_, max_)      for (I64 it_ = (max_ - 1); (it_) != 0; ++(it_))
#define For_U64_Reverse(it_, max_)      for (U64 it_ = (max_ - 1); (it_) != 0; ++(it_))

// NOTE(cmat): Defer_Scope. Works similar to D's scope(exit); the end statement is executed
// at the end of the scope. Original idea from Ryan J Fleury, RADDebugger codebase.
#define Defer_Scope_1(defer_id, start, end) for (B32 defer_id = ((start), 0); defer_id == 0; defer_id = ((end), 1))
#define Defer_Scope(start, end) Defer_Scope_1(Macro_Join(_defer_id_, __COUNTER__), start, end)


// ------------------------------------------------------------
// #-- C Stack-Allocated Array Operations

#define sarray_len(array_)           (sizeof(array_) / sizeof((array_)[0]))
#define sarray_zero(array_)          memory_fill(array_, 0, sarray_len(array_))
#define sarray_fill(array_, value)   do { For_U64(it, sarray_len(array_)) { array_[it] = value; } } while(0);

// ------------------------------------------------------------
// #-- Memory Operations

#if COMPILER_CLANG || COMPILER_GCC
force_inline cb_function void memory_copy    (void *dst, void *src, U64 bytes) { __builtin_memcpy(dst, src, bytes); }
force_inline cb_function void memory_fill    (void *dst, U08 fill, U64 bytes)  { __builtin_memset(dst, fill, bytes); }
force_inline cb_function B32  memory_compare (void *lhs, void *rhs, U64 bytes) { return __builtin_memcmp(lhs, rhs, bytes); }

#elif COMPILER_MSVC
# pragma intrinsic(__movsb)
# pragma intrinsic(__stosb)

force_inline cb_function void memory_copy    (void *dst, void *src, U64 bytes) { __movsb(dst, src, bytes); }
force_inline cb_function void memory_fill    (void *dst, U08 fill, U64 bytes)  { __stosb(dst, fill, bytes); }

inline cb_function B32 memory_compare (void *lhs, void *rhs, U64 bytes) {
  U08 *a = (U08 *)lhs;
  U08 *b = (U08 *)rhs;

  For_U64(it, bytes) {
    if (a[it] != b[it]) {
      return 0;
    }
  }
  
  return 1;
}
#endif

#define zero_fill(type_ptr) memory_fill(type_ptr, 0, sizeof(*(type_ptr)))

// ------------------------------------------------------------
// #-- Atomic Operations

#if COMPILER_GCC || COMPILER_CLANG
force_inline cb_function U32   atomic_read_u32       (volatile U32 *x)             { return __atomic_load_n(x, __ATOMIC_SEQ_CST);              }
force_inline cb_function I32   atomic_read_i32       (volatile I32 *x)             { return __atomic_load_n(x, __ATOMIC_SEQ_CST);              }
force_inline cb_function U32   atomic_write_u32      (volatile U32 *x, U32 value)  { return __atomic_exchange_n(x, value, __ATOMIC_SEQ_CST);   }
force_inline cb_function I32   atomic_write_i32      (volatile I32 *x, I32 value)  { return __atomic_exchange_n(x, value, __ATOMIC_SEQ_CST);   }
force_inline cb_function U32   atomic_increment_u32  (volatile U32 *x)             { return __atomic_fetch_add(x, 1, __ATOMIC_SEQ_CST) + 1;    }
force_inline cb_function I32   atomic_increment_i32  (volatile I32 *x)             { return __atomic_fetch_add(x, 1, __ATOMIC_SEQ_CST) + 1;    }
force_inline cb_function U32   atomic_decrement_u32  (volatile U32 *x)             { return __atomic_fetch_sub(x, 1, __ATOMIC_SEQ_CST) - 1;    }
force_inline cb_function I32   atomic_decrement_i32  (volatile I32 *x)             { return __atomic_fetch_sub(x, 1, __ATOMIC_SEQ_CST) - 1;    }

#elif COMPILER_MSVC
force_inline cb_function U32   atomic_read_u32       (volatile U32 *x)             { return *x;                                                }
force_inline cb_function I32   atomic_read_i32       (volatile I32 *x)             { return *x;                                                }
force_inline cb_function U32   atomic_write_u32      (volatile U32 *x, U32 value)  { InterlockedExchange((I32 *)x, (I32)value);                }
force_inline cb_function I32   atomic_write_i32      (volatile I32 *x, I32 value)  { InterlockedExchange(x, value);                            }
force_inline cb_function U32   atomic_increment_u32  (volatile U32 *x)             { return InterlockedIncrement((I32 *)x);                    }
force_inline cb_function I32   atomic_increment_i32  (volatile I32 *x)             { return InterlockedIncrement(x);                           }
force_inline cb_function U32   atomic_decrement_u32  (volatile U32 *x)             { return InterlockedDecrement((I32 *)x);                    }
force_inline cb_function I32   atomic_decrement_i32  (volatile I32 *x)             { return InterlockedDecrement(x);                           }
#endif

#if ARCH_X86
#include <immintrin.h>
#define spinlock_pause _mm_pause()
#elif ARCH_ARM
#define spinlock_pause __asm__ volatile("yield")
#elif ARCH_WASM
#define spinlock_pause do  { } while(0)
#endif


// ------------------------------------------------------------
// #-- Threading

typedef struct Mutex {
  volatile U32 next;
  volatile U32 serving;
} Mutex;

inline cb_function void mutex_start(Mutex *mutex) {
  U32 ticket = atomic_increment_u32(&mutex->next) - 1;
  while (atomic_read_u32(&mutex->serving) != ticket) {
    spinlock_pause;
  }
}

inline cb_function void mutex_end(Mutex *ticket) {
  atomic_increment_u32(&ticket->serving);
}

#define Mutex_Scope(mutex) Defer_Scope(mutex_start(mutex), mutex_end(mutex))

// ------------------------------------------------------------
// #-- Primitive Type Constants

#define i08_limit_min         (-127 - 1)
#define i08_limit_max         ( 127)
#define i16_limit_min         (-32767 - 1)
#define i16_limit_max         ( 32767)
#define i32_limit_min         (-2147483647 - 1)
#define i32_limit_max         ( 2147483647)
#define i64_limit_min         (-9223372036854775807LL - 1LL)
#define i64_limit_max         ( 9223372036854775807LL)

#define u08_limit_min         (0)
#define u08_limit_max         (255)
#define u16_limit_min         (0)
#define u16_limit_max         (65535)
#define u32_limit_min         (0)
#define u32_limit_max         (4294967295)
#define u64_limit_min         (0)
#define u64_limit_max         (18446744073709551615uLL)

#define f32_smallest_positive (1.17549435e-38f)
#define f64_smallest_positive (2.2250738585072014e-308)

#define f32_largest_negative  (-1.17549435e-38f)
#define f64_largest_negative  (-2.2250738585072014e-308)

#define f32_largest_positive  (3.40282346638528859812e+38f)
#define f64_largest_positive  (1.7976931348623157E+308)

#define f32_smallest_negative (3.40282346638528859812e+38f)
#define f64_smallest_negative (-1.7976931348623157E+308)

#define f32_pi  3.14159265358979323846f
#define f64_pi  3.14159265358979323846

#define f32_2pi 6.28318530717958647693f
#define f64_2pi 6.28318530717958647693

#define f32_hpi 1.57079632679489661923f
#define f64_hpi 1.57079632679489661923

// ------------------------------------------------------------
// #-- Primitive Types Core Operations

#define u32_pack(a, b, c, d)              ((U32)(d) << 24 | (U32)(c) << 16 | (U32)(b) << 8 | (U32)(a))
#define u64_pack(a, b, c, d, e, f, g, h)  ((U64)(h) << 56 | (U64)(g) << 48 | (U64)(f) << 40 | (U64)(e) << 32 | (U64)(d) << 24 | (U64)(c) << 16 | (U64)(b) << 8  | (U64)(a))

#define u64_kilobytes(x) ((U64)(x)          * 1024LL)
#define u64_megabytes(x) (u64_kilobytes(x)  * 1024LL)
#define u64_gigabytes(x) (u64_megabytes(x)  * 1024LL)
#define u64_terabytes(x) (u64_gigabytes(x)  * 1024LL)

#define u64_thousands(x)  ((U64)(x)         * 1000LL)
#define u64_millions(x)   (u64_thousands(x) * 1000LL)
#define u64_billions(x)   (u64_millions(x)  * 1000LL)
#define u64_trillions(x)  (u64_billions(x)  * 1000LL)

force_inline cb_function U08 u08_min     (U08 lhs, U08 rhs)      { return lhs < rhs ? lhs : rhs;         }
force_inline cb_function U08 u08_max     (U08 lhs, U08 rhs)      { return lhs > rhs ? lhs : rhs;         }
force_inline cb_function U08 u08_clamp   (U08 x, U08 a, U08 b)   { return u08_min(u08_max(x, a), b);     }

force_inline cb_function U16 u16_min     (U16 lhs, U16 rhs)      { return lhs < rhs ? lhs : rhs;         }
force_inline cb_function U16 u16_max     (U16 lhs, U16 rhs)      { return lhs > rhs ? lhs : rhs;         }
force_inline cb_function U16 u16_clamp   (U16 x, U16 a, U16 b)   { return u16_min(u16_max(x, a), b);     }

force_inline cb_function U32 u32_min     (U32 lhs, U32 rhs)      { return lhs < rhs ? lhs : rhs;         }
force_inline cb_function U32 u32_max     (U32 lhs, U32 rhs)      { return lhs > rhs ? lhs : rhs;         }
force_inline cb_function U32 u32_clamp   (U32 x, U32 a, U32 b)   { return u32_min(u32_max(x, a), b);     }

force_inline cb_function U64 u64_min     (U64 lhs, U64 rhs)      { return lhs < rhs ? lhs : rhs;         }
force_inline cb_function U64 u64_max     (U64 lhs, U64 rhs)      { return lhs > rhs ? lhs : rhs;         }
force_inline cb_function U64 u64_clamp   (U64 x, U64 a, U64 b)   { return u64_min(u64_max(x, a), b);     }

force_inline cb_function I08 i08_min     (I08 lhs, I08 rhs)      { return lhs < rhs ? lhs : rhs;         }
force_inline cb_function I08 i08_max     (I08 lhs, I08 rhs)      { return lhs > rhs ? lhs : rhs;         }
force_inline cb_function I08 i08_clamp   (I08 x, I08 a, I08 b)   { return i08_min(i08_max(x, a), b);     }
force_inline cb_function I08 i08_sign    (I08 x)                 { return x == 0 ? 0 : (x < 0 ? -1 : 1); }
force_inline cb_function I08 i08_abs     (I08 x)                 { return x < 0 ? -x : x;                }

force_inline cb_function I16 i16_min     (I16 lhs, I16 rhs)      { return lhs < rhs ? lhs : rhs;         }
force_inline cb_function I16 i16_max     (I16 lhs, I16 rhs)      { return lhs > rhs ? lhs : rhs;         }
force_inline cb_function I16 i16_clamp   (I16 x, I16 a, I16 b)   { return i16_min(i16_max(x, a), b);     }
force_inline cb_function I16 i16_sign    (I16 x)                 { return x == 0 ? 0 : (x < 0 ? -1 : 1); }
force_inline cb_function I16 i16_abs     (I16 x)                 { return x < 0 ? -x : x;                }

force_inline cb_function I32 i32_min     (I32 lhs, I32 rhs)      { return lhs < rhs ? lhs : rhs;         }
force_inline cb_function I32 i32_max     (I32 lhs, I32 rhs)      { return lhs > rhs ? lhs : rhs;         }
force_inline cb_function I32 i32_clamp   (I32 x, I32 a, I32 b)   { return i32_min(i32_max(x, a), b);     }
force_inline cb_function I32 i32_sign    (I32 x)                 { return x == 0 ? 0 : (x < 0 ? -1 : 1); }
force_inline cb_function I32 i32_abs     (I32 x)                 { return x < 0 ? -x : x;                }

force_inline cb_function I64 i64_min     (I64 lhs, I64 rhs)      { return lhs < rhs ? lhs : rhs;         }
force_inline cb_function I64 i64_max     (I64 lhs, I64 rhs)      { return lhs > rhs ? lhs : rhs;         }
force_inline cb_function I64 i64_clamp   (I64 x, I64 a, I64 b)   { return i64_min(i64_max(x, a), b);     }
force_inline cb_function I64 i64_sign    (I64 x)                 { return x == 0 ? 0 : (x < 0 ? -1 : 1); }
force_inline cb_function I64 i64_abs     (I64 x)                 { return x < 0 ? -x : x;                }

force_inline cb_function F32 f32_min      (F32 lhs, F32 rhs)      { return lhs < rhs ? lhs : rhs;         }
force_inline cb_function F32 f32_max      (F32 lhs, F32 rhs)      { return lhs > rhs ? lhs : rhs;         }
force_inline cb_function F32 f32_clamp    (F32 x, F32 a, F32 b)   { return f32_min(f32_max(x, a), b);     }
force_inline cb_function F32 f32_sign     (F32 x)                 { return x == 0 ? 0 : (x < 0 ? -1 : 1); }
force_inline cb_function F32 f32_abs      (F32 x)                 { return x < 0 ? -x : x;                }
force_inline cb_function F32 f32_div_safe (F32 a, F32 b)          { return b == 0 ? 0 : a / b;            }

force_inline cb_function F64 f64_min      (F64 lhs, F64 rhs)      { return lhs < rhs ? lhs : rhs;         }
force_inline cb_function F64 f64_max      (F64 lhs, F64 rhs)      { return lhs > rhs ? lhs : rhs;         }
force_inline cb_function F64 f64_clamp    (F64 x, F64 a, F64 b)   { return f64_min(f64_max(x, a), b);     }
force_inline cb_function F64 f64_sign     (F64 x)                 { return x == 0 ? 0 : (x < 0 ? -1 : 1); }
force_inline cb_function F64 f64_abs      (F64 x)                 { return x < 0 ? -x : x;                }
force_inline cb_function F64 f64_div_safe (F64 a, F64 b)          { return b == 0 ? 0 : a / b;            }

// ------------------------------------------------------------
// #-- Character Operations

inline cb_function B32 char_is_alpha      (I08 c) { return c >= 'A' && c <= 'z'; }
inline cb_function B32 char_is_number     (I08 c) { return c >= '0' && c <= '9'; }
inline cb_function B32 char_is_upper      (I08 c) { return c >= 'A' && c <= 'Z'; }
inline cb_function B32 char_is_lower      (I08 c) { return c >= 'a' && c <= 'z'; }
inline cb_function B32 char_is_whitespace (I08 c) { return c == ' ' || c == 't' || c == 'r' || c == 'n'; }
inline cb_function B32 char_is_visible    (I08 c) { return c >= '!' && c <= '~'; }
inline cb_function B32 char_is_linefeed   (I08 c) { return c == 'r' || c == 'n'; }
inline cb_function I08 char_to_upper      (I08 c) { return !char_is_alpha(c) ? c : (c >  'Z' ? c - ('A' - 'a') : c); }
inline cb_function I08 char_to_lower      (I08 c) { return !char_is_alpha(c) ? c : (c <= 'Z' ? c + ('A' - 'a') : c); }

// ------------------------------------------------------------
// #-- F32 Core Math

#if ARCH_ARM
# if COMPILER_CLANG || COMPILER_GCC

force_inline cb_function F32 f32_floor(F32 x) {
  F32 result = 0;
  __asm__ volatile ("fcvtms %w0, %s1\n" "scvtf  %s0, %w0\n" : "=&w"(result) : "w"(x));
  return result;
}

force_inline cb_function F32 f32_ceil(F32 x) {
  F32 result = 0;
  __asm__ volatile ("fcvtps %w0, %s1\n" "scvtf  %s0, %w0\n" : "=&w"(result) : "w"(x));
  return result;
}

force_inline cb_function F32 f32_sqrt(F32 x) {
  F32 result = 0;
  __asm__ volatile ("fsqrt %s0, %s1" : "=w"(result) : "w"(x));
  return result;
}
# endif

#elif ARCH_X86
# include <immintrin.h>

force_inline cb_function F32 f32_floor(F32 x) {
  return _mm_cvtss_f32(_mm_floor_ss(_mm_setzero_ps(), _mm_set_ss(x)));
}

force_inline cb_function F32 f32_ceil(F32 x) {
  return _mm_cvtss_f32(_mm_ceil_ss(_mm_setzero_ps(), _mm_set_ss(x)));
}

force_inline cb_function F32 f32_sqrt(F32 x) {
  return _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(x)));
}

#elif ARCH_WASM

force_inline cb_function F32 f32_floor(F32 x) {
  return __builtin_floorf(x);
}

force_inline cb_function F32 f32_ceil(F32 x) {
  return __builtin_ceilf(x);
}

force_inline cb_function F32 f32_sqrt(F32 x) {
  return (x);
}

#endif

force_inline cb_function F32 f32_pow2                 (F32 x) { return x * x;                 }
force_inline cb_function F32 f32_fract                (F32 x) { return x - f32_floor(x);      }
force_inline cb_function F32 f32_degrees_from_radians (F32 x) { return (x * 180.f)  / f32_pi; }
force_inline cb_function F32 f32_radians_from_degrees (F32 x) { return (x * f32_pi) / 180.0f; }

cb_function F32 f32_sin (F32 x);

force_inline cb_function F32 f32_cos  (F32 x) { return f32_sin(f32_hpi + x);   }
force_inline cb_function F32 f32_tan  (F32 x) { return f32_sin(x) / f32_cos(x); }

// ------------------------------------------------------------
// #-- Local Time

typedef struct {
  U32 year;
  U08 month;
  U08 day;
  U08 hours;
  U08 minutes;
  U08 seconds;
  U32 microseconds;
} Local_Time;

cb_function Local_Time local_time_from_unix_time(U64 unix_seconds, U64 unix_microseconds);

// ------------------------------------------------------------
// #-- Core Operating System Features

typedef struct Core_Context {
  Str cpu_name;
  U64 cpu_logical_cores;
  U64 mmu_page_bytes;
  U64 ram_capacity_bytes;
} Core_Context;

typedef U32 Core_Stream;
enum {
  Core_Stream_Standard_Output,
  Core_Stream_Standard_Error,
};

typedef U32 Core_Commit_Flag;
enum {
  Core_Commit_Flag_Read         = 1 << 0,
  Core_Commit_Flag_Write        = 1 << 1,
  Core_Commit_Flag_Executable   = 1 << 2,
};

typedef struct Core_File {
  U64 os_handle_1;
} Core_File;

typedef U32 Core_File_Access_Flag;
enum {
  Core_File_Access_Flag_Read        = 1 << 0,
  Core_File_Access_Flag_Write       = 1 << 1,

  Core_File_Access_Flag_Create      = 1 << 2,
  Core_File_Access_Flag_Truncate    = 1 << 3,
  Core_File_Access_Flag_Append      = 1 << 4,
};

typedef struct Core_File_Async_State {
  U64 os_handle_1;
} Core_File_Async_State;

cb_function Core_Context *            core_context              (void);
cb_function void                      core_stream_write         (Str buffer, Core_Stream stream);
cb_function void                      core_panic                (Str reason);
cb_function Local_Time                core_local_time           (void);

cb_function U08 *                     core_memory_reserve       (U64 bytes);
cb_function void                      core_memory_unreserve     (void *virtual_base, U64 bytes);
cb_function void                      core_memory_commit        (void *virtual_base, U64 bytes, Core_Commit_Flag mode);
cb_function void                      core_memory_uncommit      (void *virtual_base, U64 bytes);

cb_function void                      core_entry_point          (I32 arg_count, char **arg_values);

cb_function B32                       core_directory_create     (Str folder_path);
cb_function B32                       core_directory_delete     (Str folder_path);

cb_function Core_File                 core_file_open            (Str file_path, Core_File_Access_Flag flags);
cb_function void                      core_file_close           (Core_File *file);
cb_function U64                       core_file_size            (Core_File *file);

cb_function void                      core_file_read            (Core_File *file, U64 offset, U64 bytes, void *data);
cb_function void                      core_file_write           (Core_File *file, U64 offset, U64 bytes, void *data);

cb_function Core_File_Async_State     core_file_read_async      (Core_File *file, U64 offset, U64 bytes, void *data);
cb_function Core_File_Async_State     core_file_write_async     (Core_File *file, U64 offset, U64 bytes, void *data);

#define File_IO_Scope(file_, str_, mode_) Defer_Scope(*(file_) = core_file_open(str_, mode_), core_file_close(file_))

// ------------------------------------------------------------
// #-- Runtime Assertion
#if BUILD_ASSERT
# define Assert_Preamble "ASSERT (" __FILE__ ":" Macro_Stringize(__LINE__) "): "

# if COMPILER_GCC || COMPILER_CLANG
#   define Assert(condition, message) do { If_Unlikely (!(condition)) { core_panic(str_lit(Assert_Preamble " " message)); __builtin_debugtrap(); } } while(0);
# elif COMPILER_MSVC
#   define Assert(condition, message) do { If_Unlikely (!(condition)) { core_panic(str_lit(Assert_Preamble " " message)); __debugbreak(); } } while(0);
# endif
#else
# define Assert(condition, message) do { } while(0);
#endif

#define Invalid_Default default: { Assert(0, "invalid default in switch"); } break;
#define Not_Implemented Assert(0, "function not implemented");

// ------------------------------------------------------------
// #-- Address, Pointer Operations

inline cb_function U64 address_align(U64 address, U64 align) {
  Assert(!(align & (align - 1)), "align is not a power of two");

  U64 remainder = address & (align - 1);
  if (remainder) {
    address += align - remainder;
  }

  return address;
}

inline cb_function U08 *pointer_align(U08 *pointer, U64 align) {
  U64 address = (U64)pointer;
  address = address_align(address, align);
  pointer = (U08 *)address;
  return pointer;
}
