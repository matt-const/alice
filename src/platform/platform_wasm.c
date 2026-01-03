// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)


// ------------------------------------------------------------
// #-- JS - WASM platform API.
fn_external void js_platform_set_shared_memory(U32 frame_state);

#pragma pack(push, 1)

var_global struct {
  U32 display_resolution_width;
  U32 display_resolution_height;
  F32 display_frame_delta;

  U32 mouse_position_x;
  U32 mouse_position_y;

  F32 mouse_scroll_dt_x;
  F32 mouse_scroll_dt_y;

  U32 mouse_button_left;
  U32 mouse_button_right;
  U32 mouse_button_middle;

} WASM_Shared_Frame_State = { };

#pragma pack(pop)


var_global struct {
  // TODO(cmat): Having doubts, I think first frame should be userland instead.
  B32                       first_frame;
  Platform_Next_Frame_Hook *next_frame;
  Platform_Frame_State      frame_state;
} WASM_Platform_State;

fn_internal Platform_Bootstrap wasm_default_bootstrap(void) {
  Platform_Bootstrap result = {
    .title = str_lit("Alice Engine"),
    .next_frame = 0,
    .render = {
      .resolution = v2i(2560, 1440),
    },
  };

  return result;
}

fn_internal Platform_Frame_State *platform_frame_state(void) {
  return &WASM_Platform_State.frame_state;
}

fn_internal void wasm_update_input(Platform_Input *input) {
  V2F new_position          = v2f((F32)WASM_Shared_Frame_State.mouse_position_x, (F32)WASM_Shared_Frame_State.mouse_position_y);
  input->mouse.position_dt  = v2f_sub(new_position, input->mouse.position);
  input->mouse.position     = new_position;

  input->mouse.scroll_dt.x  = WASM_Shared_Frame_State.mouse_scroll_dt_x;
  input->mouse.scroll_dt.y  = WASM_Shared_Frame_State.mouse_scroll_dt_y;

  input->mouse.left.down_first_frame    = WASM_Shared_Frame_State.mouse_button_left && !input->mouse.left.down;
  input->mouse.right.down_first_frame   = WASM_Shared_Frame_State.mouse_button_right && !input->mouse.right.down;
  input->mouse.middle.down_first_frame  = WASM_Shared_Frame_State.mouse_button_middle && !input->mouse.middle.down;

  input->mouse.left.down    = WASM_Shared_Frame_State.mouse_button_left;
  input->mouse.right.down   = WASM_Shared_Frame_State.mouse_button_right;
  input->mouse.middle.down  = WASM_Shared_Frame_State.mouse_button_left;
}

fn_internal void wasm_update_frame_state(Platform_Frame_State *state) {
  state->display.frame_index += 1;
  state->display.frame_delta  = WASM_Shared_Frame_State.display_frame_delta;
  state->display.resolution   = v2f(WASM_Shared_Frame_State.display_resolution_width, WASM_Shared_Frame_State.display_resolution_height);
  state->display.aspect_ratio = f32_div_safe(state->display.resolution.x, state->display.resolution.y);

  wasm_update_input(&state->input);
}

__attribute__((export_name("wasm_next_frame")))
fn_entry void wasm_next_frame(void) {

  Platform_Render_Context render_context = {
    .backend = Platform_Render_Backend_WebGPU
  };

  wasm_update_frame_state(&WASM_Platform_State.frame_state);

  var_local_persist B32 first_frame = 1;
  WASM_Platform_State.next_frame(first_frame, &render_context);
  if (first_frame) first_frame = 0;
}

fn_internal void base_entry_point(Array_Str command_line) {
  Platform_Bootstrap boot = wasm_default_bootstrap();
  platform_entry_point(command_line, &boot);
  Assert(boot.next_frame, "next_frame not provided");

  zero_fill(&WASM_Platform_State);
  WASM_Platform_State.first_frame = 1;
  WASM_Platform_State.next_frame  = boot.next_frame;

  zero_fill(&WASM_Shared_Frame_State);
  js_platform_set_shared_memory((U32)&WASM_Shared_Frame_State);
}
