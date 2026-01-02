// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

// ------------------------------------------------------------
// #-- Platform API

typedef U32 Platform_Render_Backend;
enum {
  Platform_Render_Backend_Metal,
  Platform_Render_Backend_D3D11,
  Platform_Render_Backend_WebGPU,
  Platform_Render_Backend_OpenGL4,
};

typedef struct Platform_Render_Context {
  Platform_Render_Backend backend;

  union {
    struct { U08 *os_handle_1; U08 *os_handle_2; };
    struct { U08 *metal_device; U08 *metal_layer; };
  };
} Platform_Render_Context;

typedef void Platform_Next_Frame_Hook(B32 first_frame, Platform_Render_Context *render_context);

enum {
  Platform_Display_Resolution_Default = 0xFFFFFFFF,
  Platform_Display_Position_Centered  = 0xFFFFFFFF,
};

typedef struct Platform_Render_Setup {
  Platform_Render_Backend backend;
  V2I                     resolution;
} Platform_Render_Setup;

typedef struct Platform_Bootstrap {
  Str                       title;
  Platform_Next_Frame_Hook *next_frame;
  Platform_Render_Setup     render;
} Platform_Bootstrap;

typedef struct Platform_Button {
  B32 down;
  B32 down_first_frame;
} Platform_Button;

typedef struct Platform_Mouse {
  V2F position;
  V2F position_dt;
  V2F scroll_dt;

  union {
    Platform_Button buttons[3];
    struct {
      Platform_Button left;
      Platform_Button middle;
      Platform_Button right;
    };
  };

} Platform_Mouse;

typedef struct Platform_Input {
  Platform_Mouse mouse;
} Platform_Input;

typedef struct Platform_Display {
  U64 frame_index;
  F32 frame_delta;
  V2F resolution;
  F32 aspect_ratio;
} Platform_Display;

typedef struct Platform_Frame_State {
  Platform_Input          input;
  Platform_Display        display;
} Platform_Frame_State;

// NOTE(cmat): Returns the current frame state (input, render context, display info, etc.)
// These can only be called in the next_frame hook, on the render thread.
fn_internal Platform_Frame_State *platform_frame_state(void);

force_inline fn_internal Platform_Input           *platform_input           (void)  { return &platform_frame_state()->input;            }
force_inline fn_internal Platform_Display         *platform_display         (void)  { return &platform_frame_state()->display;          }
force_inline fn_internal R2I                       platform_display_region  (void)  { return r2i_v(v2i(0, 0), v2i((I32)platform_display()->resolution.x, (I32)platform_display()->resolution.y)); }


// ------------------------------------------------------------
// #-- Entry Point

fn_internal void platform_entry_point(Array_Str command_line, Platform_Bootstrap *boot);
