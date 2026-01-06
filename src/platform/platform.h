// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

typedef U32 PL_KB_Code;
enum {
  PL_KB_Shift_Left       = 0,
  PL_KB_Shift_Right      = 1,
  PL_KB_Control_Left     = 2,
  PL_KB_Control_Right    = 3,
  PL_KB_Alt_Left         = 4,
  PL_KB_Alt_Right        = 5,
  PL_KB_Meta_Left        = 6,
  PL_KB_Meta_Right       = 7,
  PL_KB_Caps_Lock        = 8,
  PL_KB_Num_Lock         = 9,
  PL_KB_Scroll_Lock      = 10,

  PL_KB_Enter            = 11,
  PL_KB_Escape           = 12,
  PL_KB_Backspace        = 13,
  PL_KB_Tab              = 14,
  PL_KB_Space            = 15,

  PL_KB_A                = 16,
  PL_KB_B                = 17,
  PL_KB_C                = 18,
  PL_KB_D                = 19,
  PL_KB_E                = 20,
  PL_KB_F                = 21,
  PL_KB_G                = 22,
  PL_KB_H                = 23,
  PL_KB_I                = 24,
  PL_KB_J                = 25,
  PL_KB_K                = 26,
  PL_KB_L                = 27,
  PL_KB_M                = 28,
  PL_KB_N                = 29,
  PL_KB_O                = 30,
  PL_KB_P                = 31,
  PL_KB_Q                = 32,
  PL_KB_R                = 33,
  PL_KB_S                = 34,
  PL_KB_T                = 35,
  PL_KB_U                = 36,
  PL_KB_V                = 37,
  PL_KB_W                = 38,
  PL_KB_X                = 39,
  PL_KB_Y                = 40,
  PL_KB_Z                = 41,

  PL_KB_0                = 42,
  PL_KB_1                = 43,
  PL_KB_2                = 44,
  PL_KB_3                = 45,
  PL_KB_4                = 46,
  PL_KB_5                = 47,
  PL_KB_6                = 48,
  PL_KB_7                = 49,
  PL_KB_8                = 50,
  PL_KB_9                = 51,

  PL_KB_Minus            = 52,
  PL_KB_Equal            = 53,
  PL_KB_Bracket_Left     = 54,
  PL_KB_Bracket_Right    = 55,
  PL_KB_Backslash        = 56,
  PL_KB_Semicolon        = 57,
  PL_KB_Quote            = 58,
  PL_KB_Backquote        = 59,
  PL_KB_Comma            = 60,
  PL_KB_Period           = 61,
  PL_KB_Slash            = 62,

  PL_KB_Arrow_Left       = 63,
  PL_KB_Arrow_Right      = 64,
  PL_KB_Arrow_Up         = 65,
  PL_KB_Arrow_Down       = 66,
  PL_KB_Home             = 67,
  PL_KB_End              = 68,
  PL_KB_Page_Up          = 69,
  PL_KB_Page_Down        = 70,
  PL_KB_Insert           = 71,
  PL_KB_Delete           = 72,

  PL_KB_F1               = 73,
  PL_KB_F2               = 74,
  PL_KB_F3               = 75,
  PL_KB_F4               = 76,
  PL_KB_F5               = 77,
  PL_KB_F6               = 78,
  PL_KB_F7               = 79,
  PL_KB_F8               = 80,
  PL_KB_F9               = 81,
  PL_KB_F10              = 82,
  PL_KB_F11              = 83,
  PL_KB_F12              = 84,

  PL_KB_Count            = 85
};


// ------------------------------------------------------------
// #-- PL API

typedef U32 PL_Render_Backend;
enum {
  PL_Render_Backend_Metal,
  PL_Render_Backend_D3D11,
  PL_Render_Backend_WebGPU,
  PL_Render_Backend_OpenGL4,
};

typedef struct PL_Render_Context {
  PL_Render_Backend backend;

  union {
    struct { U08 *os_handle_1; U08 *os_handle_2; };
    struct { U08 *metal_device; U08 *metal_layer; };
  };
} PL_Render_Context;

typedef void PL_Next_Frame_Hook(B32 first_frame, PL_Render_Context *render_context);

enum {
  PL_Display_Resolution_Default = 0xFFFFFFFF,
  PL_Display_Position_Centered  = 0xFFFFFFFF,
};

typedef struct PL_Render_Setup {
  PL_Render_Backend backend;
  V2I                     resolution;
} PL_Render_Setup;

typedef struct PL_Bootstrap {
  Str                       title;
  PL_Next_Frame_Hook *next_frame;
  PL_Render_Setup     render;
} PL_Bootstrap;

typedef struct PL_Button {
  B32 down;
  B32 down_first_frame;
} PL_Button;

typedef struct PL_Mouse {
  V2F position;
  V2F position_dt;
  V2F scroll_dt;

  union {
    PL_Button buttons[3];
    struct {
      PL_Button left;
      PL_Button middle;
      PL_Button right;
    };
  };

} PL_Mouse;

typedef struct PL_Keyboard {
  U08 *state;
} PL_Keyboard;

typedef struct PL_Input {
  PL_Mouse    mouse;
  PL_Keyboard keyboard;
} PL_Input;

typedef struct PL_Display {
  U64 frame_index;
  F32 frame_delta;
  V2F resolution;
  F32 aspect_ratio;
} PL_Display;

typedef struct PL_Frame_State {
  PL_Input          input;
  PL_Display        display;
} PL_Frame_State;

// NOTE(cmat): Returns the current frame state (input, render context, display info, etc.)
// These can only be called in the next_frame hook, on the render thread.
fn_internal PL_Frame_State *pl_frame_state(void);

force_inline fn_internal PL_Input   *pl_input           (void)  { return &pl_frame_state()->input;            }
force_inline fn_internal PL_Display *pl_display         (void)  { return &pl_frame_state()->display;          }
force_inline fn_internal R2I        pl_display_region  (void)  { return r2i_v(v2i(0, 0), v2i((I32)pl_display()->resolution.x, (I32)pl_display()->resolution.y)); }


// ------------------------------------------------------------
// #-- Entry Point

fn_internal void pl_entry_point(Array_Str command_line, PL_Bootstrap *boot);
