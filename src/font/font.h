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
  V2_U16         glyph_atlas_size;
  R_Texture      glyph_atlas;
  U64            glyph_bucket_count;
  FO_Glyph_List *glyph_bucket_array;
} FO_Font;

fn_internal void fo_font_init(FO_Font *font, Arena *arena, Str font_data, I32 font_size, V2_U16 atlas_size, Array_Codepoint codepoints);

fn_internal FO_Glyph *fo_font_glyph_add(FO_Font *font, Arena *arena, Codepoint codepoint);
fn_internal FO_Glyph *fo_font_glyph_get(FO_Font *font, Codepoint codepoint);

U32 Codepoints_ASCII_Data[] = {
  32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53,
  54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75,
  76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97,
  98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115,
  116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126,
};

Assert_Compiler(sarray_len(Codepoints_ASCII_Data) == 127 - 32);
Array_Codepoint Codepoints_ASCII = array_from_sarray(Array_Codepoint, Codepoints_ASCII_Data);
