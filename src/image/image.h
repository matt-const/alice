// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

typedef struct IM_Bitmap {
  U32  width;
  U32  height;
  U32  channels;
  U08 *dat;
} IM_Bitmap;

typedef U32 IM_File_Format;
enum {
  IM_File_Format_TGA,

  IM_File_Format_Count
};

cb_function inline IM_Bitmap  im_bitmap_allocate    (Arena *arena, U32 width, U32 height, U32 channels);
cb_function inline void       im_bitmap_write_file  (IM_Bitmap *bitmap, Str filepath, IM_File_Format format);
