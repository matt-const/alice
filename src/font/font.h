// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)


typedef struct FO_Glyph {
  struct FO_Glyph  *hash_next;

  Codepoint codepoint;
  B32       no_texture;

  V2I       bounds;
  R2F       atlas_uv;
  V2I       pen_offset;
  I32       pen_advance;
} FO_Glyph;

typedef struct FO_Glyph_List {
  FO_Glyph *first;
  FO_Glyph *last;
} FO_Glyph_List;

typedef struct FO_Font {
  I32            metric_em;
  I32            metric_ascent;
  I32            metric_descent;
  I32            metric_height; // NOTE(cmat): ascent + (-descent)
  I32            metric_line_gap;

  V2_U16         glyph_atlas_size;
  R_Texture_2D   glyph_atlas;
  U64            glyph_bucket_count;
  FO_Glyph_List *glyph_bucket_array;
} FO_Font;

fn_internal void fo_font_init(FO_Font *font, Arena *arena, Str font_data, I32 font_size, V2_U16 atlas_size, Array_Codepoint codepoints);

fn_internal FO_Glyph *fo_glyph_add(FO_Font *font, Arena *arena, Codepoint codepoint);
fn_internal FO_Glyph *fo_glyph_get(FO_Font *font, Codepoint codepoint);
fn_internal F32       fo_text_width(FO_Font *font, Str text);

inline fn_internal F32 fo_em(FO_Font *font, F32 em) {
  F32 result = 0;
  if (font) {
    result = font->metric_em * em;
  }

  return result;
}

var_global U32 Codepoints_ASCII_Data[] = {
  32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53,
  54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75,
  76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97,
  98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115,
  116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126,
};

Assert_Compiler(sarray_len(Codepoints_ASCII_Data) == 127 - 32);
Array_Codepoint Codepoints_ASCII = array_from_sarray(Array_Codepoint, Codepoints_ASCII_Data);

var_global U32 Codepoints_JP_Kana_Data[] = {

    // NOTE(cmat): Hiragana
    0x3041, 0x3042, 0x3043, 0x3044, 0x3045, 0x3046, 0x3047, 0x3048, 0x3049, 0x304A,
    0x304B, 0x304C, 0x304D, 0x304E, 0x304F, 0x3050, 0x3051, 0x3052, 0x3053, 0x3054,
    0x3055, 0x3056, 0x3057, 0x3058, 0x3059, 0x305A, 0x305B, 0x305C, 0x305D, 0x305E,
    0x305F, 0x3060, 0x3061, 0x3062, 0x3064, 0x3065, 0x3066, 0x3067, 0x3068, 0x3069,
    0x306A, 0x306B, 0x306C, 0x306D, 0x306E, 0x306F, 0x3070, 0x3071, 0x3072, 0x3073,
    0x3074, 0x3075, 0x3076, 0x3077, 0x3078, 0x3079, 0x307A, 0x307B, 0x307C, 0x307D,
    0x307E, 0x307F, 0x3080, 0x3081, 0x3082, 0x3083, 0x3084, 0x3085, 0x3086, 0x3087,
    0x3088, 0x3089, 0x308A, 0x308B, 0x308C, 0x308D, 0x308E, 0x308F, 0x3090, 0x3091,
    0x3092, 0x3093, 0x3094, 0x3095, 0x3096,

    // NOTE(cmat): Katakana
    0x30A1, 0x30A2, 0x30A3, 0x30A4, 0x30A5, 0x30A6, 0x30A7, 0x30A8, 0x30A9, 0x30AA,
    0x30AB, 0x30AC, 0x30AD, 0x30AE, 0x30AF, 0x30B0, 0x30B1, 0x30B2, 0x30B3, 0x30B4,
    0x30B5, 0x30B6, 0x30B7, 0x30B8, 0x30B9, 0x30BA, 0x30BB, 0x30BC, 0x30BD, 0x30BE,
    0x30BF, 0x30C0, 0x30C1, 0x30C2, 0x30C3, 0x30C4, 0x30C5, 0x30C6, 0x30C7, 0x30C8,
    0x30C9, 0x30CA, 0x30CB, 0x30CC, 0x30CD, 0x30CE, 0x30CF, 0x30D0, 0x30D1, 0x30D2,
    0x30D3, 0x30D4, 0x30D5, 0x30D6, 0x30D7, 0x30D8, 0x30D9, 0x30DA, 0x30DB, 0x30DC,
    0x30DD, 0x30DE, 0x30DF, 0x30E0, 0x30E1, 0x30E2, 0x30E3, 0x30E4, 0x30E5, 0x30E6,
    0x30E7, 0x30E8, 0x30E9, 0x30EA, 0x30EB, 0x30EC, 0x30ED, 0x30EE, 0x30EF, 0x30F0,
    0x30F1, 0x30F2, 0x30F3, 0x30F4, 0x30F5, 0x30F6, 0x30FC, 0x30FD, 0x30FE,

    // NOTE(cmat): Common punctuation
    0x3001, 0x3002, 0x300C, 0x300D, 0x300E, 0x300F, 0x301C, 0x30FB, 0x2025, 0x2026, 0x2015,
};

Array_Codepoint Codepoints_JP_Kana = array_from_sarray(Array_Codepoint, Codepoints_JP_Kana_Data);
