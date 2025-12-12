// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

// ------------------------------------------------------------
// #-- TARGA Format

#pragma pack(push, 1)

typedef struct IM_TGA_Header {
  U08 id_length;
  U08 color_map_type;
  U08 data_type_code;
  U16 color_map_origin;
  U16 color_map_length;
  U08 color_map_depth;
  U16 x_origin;
  U16 y_origin;
  U16 width;
  U16 height;
  U16 bits_per_pixel;
  U16 image_descriptor;

} IM_TGA_Header;

#pragma pack(pop)

fn_internal inline void im_bitmap_write_file_tga(IM_Bitmap *bitmap, Core_File *file) {
  if (bitmap->channels != 3) Not_Implemented;

  IM_TGA_Header header = {
    .id_length           = 0,
    .color_map_type      = 0,
    .data_type_code      = 2, // NOTE(cmat): Uncompressed RGB
    .color_map_origin    = 0,
    .color_map_length    = 0,
    .color_map_depth     = 0,
    .x_origin            = 0,
    .y_origin            = 0,
    .width               = (U16)bitmap->width,
    .height              = (U16)bitmap->height,
    .bits_per_pixel      = 24,
    .image_descriptor    = 0,
  };

  U64  header_bytes = sizeof(header);
  U64  image_bytes  = 3 * sizeof(U08) * bitmap->width * bitmap->height;

  core_file_write(file, 0,            header_bytes, &header);
  core_file_write(file, header_bytes, image_bytes,  bitmap->dat);
}

// ------------------------------------------------------------
// #-- Image API

fn_internal inline IM_Bitmap im_bitmap_allocate(Arena *arena, U32 width, U32 height, U32 channels) {
  IM_Bitmap result = {
    .width    = width,
    .height   = height,
    .channels = channels,
    .dat      = arena_push_count(arena, U08, width * height * channels),
  };

  return result;
}

fn_internal inline void im_bitmap_write_file(IM_Bitmap *bitmap, Str filepath, IM_File_Format format) {
  Core_File file = { };
  File_IO_Scope(&file, filepath, Core_File_Access_Flag_Create | Core_File_Access_Flag_Truncate | Core_File_Access_Flag_Write) {
    switch (format) {
      case IM_File_Format_TGA: { im_bitmap_write_file_tga(bitmap, &file); } break;
    }
  }
}

