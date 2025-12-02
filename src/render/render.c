// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

// ------------------------------------------------------------
// #-- Default Resources

R_Shader   R_Shader_Invalid   = {};
R_Buffer   R_Buffer_Invalid   = {};
R_Texture  R_Texture_Invalid  = {};
R_Sampler  R_Sampler_Invalid  = {};
R_Pipeline R_Pipeline_Invalid = {};

// ------------------------------------------------------------
// #-- Render Commands

R_Command_Buffer R_Commands = {};

cb_function void r_command_reset(void) {
  R_Commands.first  = 0;
  R_Commands.last   = 0;
  arena_clear(&R_Commands.arena);
}

cb_function U08 *r_command_push(R_Command_Type type, U64 bytes) {
  cb_local_persist B32 buffer_initialized = 0;
  if (!buffer_initialized) {
    buffer_initialized = 1;
    arena_init(&R_Commands.arena);
  }

  R_Command_Header *header = (R_Command_Header *)arena_push_size(&R_Commands.arena, bytes + sizeof(R_Command_Header));
  queue_push(R_Commands.first, R_Commands.last, header);
  return ((U08 *)header) + sizeof(R_Command_Header);
}

cb_function void r_command_draw(R_Command_Draw *draw) {
  memory_copy(r_command_push(R_Command_Type_Draw, sizeof(R_Command_Draw)), draw, sizeof(R_Command_Draw));
}


