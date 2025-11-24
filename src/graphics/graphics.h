// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

// ------------------------------------------------------------
// #-- 2D Immediate Mode API
#define G2_Clip_None r2i(0, 0, i32_limit_max, i32_limit_max)

cb_function void g2_init        (void);
cb_function void g2_frame_flush (void);
cb_function void g2_clip_region (R2I region);

typedef struct G2_Tri {
  V2F       x1, x2, x3;
  V2F       u1, u2, u3;
  RGBA      color;
  R_Texture tex;
} G2_Tri;

// NOTE(cmat): Triangle

cb_function void g2_draw_tri_ext(G2_Tri *tri);
#define g2_draw_tri(x1_, x2_, x3_, ...)       \
  g2_draw_tri_ext(&(G2_Tri) {                 \
      .x1       = x1_,                        \
      .x2       = x2_,                        \
      .x3       = x3_,                        \
      .u1       = v2f(0.f, 0.f),              \
      .u2       = v2f(1.f, 0.f),              \
      .u3       = v2f(0.f, 1.f),              \
      .color    = v4f(1, 1, 1, 1),            \
      .tex      = R_Texture_White,            \
      __VA_ARGS__,                            \
  })

// NOTE(cmat): Rectangle

typedef struct G2_Rect {
  V2F       pos;
  V2F       size;
  V2F       uv_bl;
  V2F       uv_tr;
  RGBA      color;
  R_Texture tex;
} G2_Rect;

cb_function void g2_draw_rect_ext(G2_Rect *rect);
#define g2_draw_rect(pos_, size_, ...)        \
  g2_draw_rect_ext(&(G2_Rect) {               \
      .pos            = pos_,                 \
      .size           = size_,                \
      .uv_bl          = v2f(0, 0),            \
      .uv_tr          = v2f(1, 1),            \
      .color          = v4f(1, 1, 1, 1),      \
      .tex            = R_Texture_White,      \
      __VA_ARGS__,                            \
  })

// NOTE(cmat): Rounded-Rectangle

typedef struct G2_Rect_Rounded {
  F32       radius;
  F32       resolution;
  V2F       pos;
  V2F       size;
  RGBA      color;
} G2_Rect_Rounded;

cb_function void g2_draw_rounded_rect_ext(G2_Rect_Rounded *rect);
#define g2_draw_rect_rounded(pos_, size_, radius_, ...)       \
  g2_draw_rect_rounded_ext(&(G2_Rect) {                       \
      .radius         = radius_,                              \
      .resolution     = 32,                                   \
      .pos            = pos_,                                 \
      .size           = size_,                                \
      .uv_bl          = v2f(0, 0),                            \
      .uv_tr          = v2f(1, 1),                            \
      .color          = v4f(1, 1, 1, 1),                      \
      .tex            = R_Texture_White,                      \
      __VA_ARGS__,                                            \
  })

// NOTE(cmat): Circle

typedef struct G2_Disk {
  V2F       pos;
  F32       radius;
  F32       resolution;
  RGBA      color;
} G2_Disk;

cb_function void g2_draw_disk_ext(G2_Disk *disk);
#define g2_draw_disk(pos_, radius_, ...)                      \
  g2_draw_disk_ext(&(G2_Disk) {                               \
      .pos            = pos_,                                 \
      .radius         = radius_,                              \
      .resolution     = 0,                                    \
      .color          = v4f(1, 1, 1, 1),                      \
      __VA_ARGS__                                             \
  })

// NOTE(cmat): Line

typedef struct G2_Line {
  V2F       start;
  V2F       end;
  F32       thickness;
  RGBA      color;
} G2_Line;

cb_function void g2_draw_line_ext(G2_Line *line);
#define g2_draw_line(start_, end_, ...)                     \
  g2_draw_line_ext(&(G2_Line) {                             \
      .start = start_,                                      \
      .end   = end_,                                        \
      .thickness      = 2.f,                                \
      .color          = v4f(1.f, 1.f, 1.f, 1.f),            \
      __VA_ARGS__                                           \
  });

// NOTE(cmat): Text

typedef struct G2_Text {
  Str      text;
  FO_Font *font;
  V2F      pos;
  F32      height;
  V4F      color;
  F32      rot_deg;
} G2_Text;

cb_function void g2_draw_text_ext(G2_Text *text);
#define g2_draw_text(text_, font_, pos_, height_, ...)    \
  g2_draw_text_ext(&(G2_Text) {                           \
      .text    = text_,                                   \
      .font    = font_,                                   \
      .pos     = pos_,                                    \
      .height  = height_,                                 \
      .color   = v4f(1.f, 1.f, 1.f, 1.f),                 \
      .rot_deg = 0.f,                                     \
      __VA_ARGS__                                         \
  });


