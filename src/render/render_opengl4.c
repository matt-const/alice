// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

R_Shader  R_Shader_Flat_2D        = { };
R_Shader  R_Shader_Flat_3D        = { };
R_Shader  R_Shader_MTSDF_2D       = { };
R_Texture R_Texture_White         = { };
R_Sampler R_Sampler_Linear_Clamp  = { };
R_Sampler R_Sampler_Nearest_Clamp = { };

#define OGL4_Max_Buffers   1024
#define OGL4_Max_Shaders   256
#define OGL4_Max_Samplers  64
#define OGL4_Max_Textures  1024
#define OGL4_Max_Pipelines 512

cb_global struct {
  B32 initialized;
 
  U32 buffer_last_id;
  U32 shader_last_id;
  U32 sampler_last_id;
  U32 texture_last_id;
  U32 pipeline_last_id;

  OGL4_Buffer   buffers   [OGL4_Max_Buffers];
  OGL4_Shader   shaders   [OGL4_Max_Shaders];
  OGL4_Sampler  samplers  [OGL4_Max_Samplers];
  OGL4_Texture  textures  [OGL4_Max_Textures];
  OGL4_Pipeline pipelines [OGL4_Max_Pipelines];

} OGL4_State;

cb_function R_Buffer r_buffer_allocate(U64 capacity, R_Buffer_Mode mode) {
}

