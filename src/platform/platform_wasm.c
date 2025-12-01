// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

// ------------------------------------------------------------

cb_global struct {
  // TODO(cmat): Having doubts, I think first frame should be userland instead.
  B32                       first_frame;
  Platform_Next_Frame_Hook *next_frame;
  Platform_Frame_State      frame_state;
} WASM_Platform_State;

cb_function Platform_Bootstrap wasm_default_bootstrap(void) {
  Platform_Bootstrap result = {
    .title = str_lit("Alice Engine"),
    .next_frame = 0,
    .render = {
      .resolution = v2i(2560, 1440),
    },
  };

  return result;
}

cb_function Platform_Frame_State *platform_frame_state(void) {
  return &WASM_Platform_State.frame_state;
}

__attribute__((export_name("wasm_next_frame")))
void wasm_next_frame(void) {

  Platform_Render_Context render_context = {
    .backend = Platform_Render_Backend_WebGPU
  };

  cb_local_persist B32 first_frame = 1;
  WASM_Platform_State.next_frame(first_frame, &render_context);
  if (first_frame) first_frame = 0;
}

cb_function void base_entry_point(Array_Str command_line) {
  Platform_Bootstrap boot = wasm_default_bootstrap();
  platform_entry_point(command_line, &boot);
  Assert(boot.next_frame, "next_frame not provided");

  zero_fill(&WASM_Platform_State);
  WASM_Platform_State.first_frame = 1;
  WASM_Platform_State.next_frame  = boot.next_frame;
}
