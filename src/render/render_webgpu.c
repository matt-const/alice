// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

// ------------------------------------------------------------
// #-- Default handles.

R_Shader  R_Shader_Flat_2D        = { };
R_Shader  R_Shader_Flat_3D        = { };
R_Shader  R_Shader_DVR_3D         = { };

R_Texture R_Texture_White         = { };
R_Sampler R_Sampler_Linear_Clamp  = { };
R_Sampler R_Sampler_Nearest_Clamp = { };

// ------------------------------------------------------------
// #-- WASM - JS WebGPU API.

fn_external U32  js_webgpu_buffer_allocate   (U32 capacity, U32 mode);
fn_external void js_webgpu_buffer_download   (U32 buffer_handle, U32 offset, U32 bytes, void *data);
fn_external void js_webgpu_buffer_destroy    (U32 buffer_handle);

fn_external U32  js_webgpu_texture_allocate  (U32 format, U32 width, U32 height);
fn_external U32  js_webgpu_texture_download  (U32 texture_handle, U32 download_format, U32 x0, U32 y0, U32 x1, U32 y1, void *data);
fn_external U32  js_webgpu_texture_destroy   (U32 texture_handle);

fn_external U32  js_webgpu_sampler_create    (U32 near_mode, U32 far_mode);
fn_external void js_webgpu_sampler_destroy   (U32 sampler_handle);

fn_external U32  js_webgpu_shader_create     (U32 string_len, void *string_ptr);
fn_external U32  js_webgpu_shader_destroy    (U32 shader_handle);

fn_external U32  js_webgpu_pipeline_create   (U32 shader_handle, void *vertex_format_ptr, B32 depth_buffer);
fn_external U32  js_webgpu_pipeline_destroy  (U32 pipeline_handle);

fn_external void js_webgpu_frame_flush       (void *command_draw_ptr);

// ------------------------------------------------------------
// #-- Built-in shaders.

var_global U08 webgpu_shader_source_flat_2D_dat[] = {
#embed "flat_2D.wgsl"
};

var_global Str webgpu_shader_source_flat_2D = {
  .len = sizeof(webgpu_shader_source_flat_2D_dat),
  .txt = webgpu_shader_source_flat_2D_dat,
};


var_global U08 webgpu_shader_source_flat_3D_dat[] = {
#embed "flat_3D.wgsl"
};

var_global Str webgpu_shader_source_flat_3D = {
  .len = sizeof(webgpu_shader_source_flat_3D_dat),
  .txt = webgpu_shader_source_flat_3D_dat,
};

var_global U08 webgpu_shader_source_grid_3D_dat[] = {
#embed "grid_3D.wgsl"
};

var_global Str webgpu_shader_source_grid_3D = {
  .len = sizeof(webgpu_shader_source_grid_3D_dat),
  .txt = webgpu_shader_source_grid_3D_dat,
};

var_global U08 webgpu_shader_source_dvr_3D_dat[] = {
#embed "dvr_3D.wgsl"
};

var_global Str webgpu_shader_source_dvr_3D = {
  .len = sizeof(webgpu_shader_source_dvr_3D_dat),
  .txt = webgpu_shader_source_dvr_3D_dat,
};


// ------------------------------------------------------------
// #-- Render API implementation.

fn_internal R_Buffer r_buffer_allocate(U64 capacity, R_Buffer_Mode mode) {
  R_Buffer result = js_webgpu_buffer_allocate(capacity, mode);
  return result;
}

fn_internal void r_buffer_download(R_Buffer buffer, U64 offset, U64 bytes, void *data) {
  js_webgpu_buffer_download(buffer, (U32)offset, (U32)bytes, data);
}

fn_internal void r_buffer_destroy(R_Buffer *buffer) {
  js_webgpu_buffer_destroy(*buffer);
  *buffer = 0;
}

fn_internal R_Texture r_texture_allocate(R_Texture_Format format, U32 width, U32 height) {
  R_Texture result = js_webgpu_texture_allocate(format, width, height);
  return result;
}

fn_internal void r_texture_download(R_Texture texture, R_Texture_Format download_format, R2I region, void *data) {
  js_webgpu_texture_download(texture, download_format, region.x0, region.y0, region.x1, region.y1, data);
}

fn_internal void r_texture_destroy(R_Texture *texture) {
  js_webgpu_texture_destroy(*texture);
  *texture = 0;
}

fn_internal R_Sampler r_sampler_create(R_Sampler_Filter mag_filter, R_Sampler_Filter min_filter) {
  R_Sampler sampler = js_webgpu_sampler_create(mag_filter, min_filter);
  return sampler;
}

fn_internal void r_sampler_destroy(R_Sampler *sampler) {
  js_webgpu_sampler_destroy(*sampler);
  *sampler = 0;
}

fn_internal R_Pipeline r_pipeline_create(R_Shader shader, R_Vertex_Format *format, B32 depth_buffer) {
  R_Pipeline pipeline = js_webgpu_pipeline_create(shader, format, depth_buffer);
  return pipeline;
}

fn_internal void r_pipeline_destroy(R_Pipeline *pipeline) {
  js_webgpu_pipeline_destroy(*pipeline);
  *pipeline = 0;
}

// ------------------------------------------------------------
// #-- WebGPU Initialization.

fn_internal void webgpu_create_default_shaders(void) {
  R_Shader_Flat_2D = js_webgpu_shader_create((U32)webgpu_shader_source_flat_2D.len, webgpu_shader_source_flat_2D.txt);
  R_Shader_Flat_3D = js_webgpu_shader_create((U32)webgpu_shader_source_flat_3D.len, webgpu_shader_source_flat_3D.txt);
  R_Shader_Grid_3D = js_webgpu_shader_create((U32)webgpu_shader_source_grid_3D.len, webgpu_shader_source_grid_3D.txt);
  R_Shader_DVR_3D  = js_webgpu_shader_create((U32)webgpu_shader_source_dvr_3D.len,  webgpu_shader_source_dvr_3D.txt);
}

fn_internal void webgpu_create_default_textures(void) {
  U32 white_texture_data[] = {
    0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF,
  };

  R_Texture_White = r_texture_allocate(R_Texture_Format_RGBA_U08_Normalized, 2, 2);
  r_texture_download(R_Texture_White, R_Texture_Format_RGBA_U08_Normalized, r2i(0, 0, 2, 2), (U08 *)white_texture_data);
}

fn_internal void webgpu_create_default_samplers(void) {
  R_Sampler_Linear_Clamp  = r_sampler_create(R_Sampler_Filter_Linear,  R_Sampler_Filter_Linear);
  R_Sampler_Nearest_Clamp = r_sampler_create(R_Sampler_Filter_Nearest, R_Sampler_Filter_Nearest);
}

fn_internal void r_init(PL_Render_Context *render_context) {
  webgpu_create_default_shaders();
  webgpu_create_default_textures();
  webgpu_create_default_samplers();
}

// ------------------------------------------------------------
// #-- WebGPU Command Submission.

fn_internal void r_frame_flush(void) {
          
  for(R_Command_Header *it = R_Commands.first; it; it = it->next) {
    switch (it->type) {
      case R_Command_Type_Draw: {
        R_Command_Draw *draw = (R_Command_Draw *)pointer_offset_bytes(it, sizeof(R_Command_Header));
        js_webgpu_frame_flush(draw);
      } break;
    }
  }

  r_command_reset();
}
