// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

typedef struct FO_Font {
  R_Texture atlas;

} FO_Font;

#if 0

// TODO(cmat): Very basic, primitive, dumb font
// implementation. Use freetype for desktop, stb for web.
// TODO(cmat): Do NOT ship this.

#include <stdio.h>

#define STB_IMAGE_IMPLEMENTATION
#include "thirdparty/stb_image.h"

#pragma pack(push, 1)
typedef struct FO_Font_Glyph {
  U32 unicode;
  F32 advance;
  R2F glyph_bounds;
  R2F atlas_bounds;
} FO_Font_Glyph;
#pragma pack(pop)

typedef struct FO_Font {
  V2I            atlas_dimension;
  R_Texture      atlas_texture;
  U32            glyph_count;
  FO_Font_Glyph *glyph_array;
  U32           *glyph_map;
  U32           *ascii_range_from;
} FO_Font;

inline cb_function FO_Font font_load(Arena *arena, char *font_atlas, char *font_glyph_map) { 
  FO_Font font;
  zero_fill(&font);

  // NOTE(cmat): Download font atlas.
  // #--
  stbi_set_flip_vertically_on_load(1);
  U08 *atlas_data = stbi_load(font_atlas, &font.atlas_dimension.x, &font.atlas_dimension.y, 0, 4);
  font.atlas_texture = r_texture_allocate(&(R_Texture_Config) {
    .format = R_Texture_Format_RGBA_U08_Normalized,
    .width  = font.atlas_dimension.x,
    .height = font.atlas_dimension.y
  });

  r_texture_download(&font.atlas_texture, atlas_data);

  // NOTE(cmat): Download glyph metadata.
  // #--
  FILE *in = fopen(font_glyph_map, "rb");
  fseek(in, 0, SEEK_END);
  U64 glyph_bytes = ftell(in);
  fseek(in, 0, SEEK_SET);

  fread(&font.glyph_count, sizeof(U32), 1, in);
  font.glyph_array = (FO_Font_Glyph *)arena_push_count(arena, FO_Font_Glyph, font.glyph_count);
  fread(font.glyph_array, sizeof(FO_Font_Glyph) * font.glyph_count, 1, in);
  fclose(in);

  For_U32(it, font.glyph_count) {
    FO_Font_Glyph *g = font.glyph_array + it;
    g->atlas_bounds.min.x /= font.atlas_dimension.x;
    g->atlas_bounds.min.y /= font.atlas_dimension.y;
    g->atlas_bounds.max.x /= font.atlas_dimension.x;
    g->atlas_bounds.max.y /= font.atlas_dimension.y;
  }
  
  font.ascii_range_from = 0;

  return font;
}

inline cb_function F32 font_text_width(FO_Font *font, Str string, F32 scale) {
  F32 text_width = 0;
  For_U32 (it, string.len) {
    FO_Font_Glyph *g = &font->glyph_array[string.txt[it] - 32];
    text_width += scale * g->advance;
  }

  return text_width;
}



#endif
