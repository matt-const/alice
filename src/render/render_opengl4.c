// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)


// NOTE(cmat): OGL4 prototypes to be loaded.
var_global PFNGLCREATEBUFFERSPROC      glCreateBuffers;
var_global PFNGLBINDBUFFERPROC         glBindBuffer;
var_global PFNGLBUFFERDATAPROC         glBufferData;
var_global PFNGLBUFFERSUBDATAPROC      glBufferSubData;
var_global PFNGLCREATETEXTURESPROC     glCreateTextures;
var_global PFNGLCREATESHADERPROC       glCreateShader;
var_global PFNGLSHADERSOURCEPROC       glShaderSource;
var_global PFNGLCOMPILESHADERPROC      glCompileShader;
var_global PFNGLATTACHSHADERPROC       glAttachShader;
var_global PFNGLCREATEPROGRAMPROC      glCreateProgram;
var_global PFNGLLINKPROGRAMPROC        glLinkProgram;
var_global PFNGLDETACHSHADERPROC       glDetachShader;
var_global PFNGLDELETESHADERPROC       glDeleteShader;

var_global PFNGLGETSHADERIVPROC        glGetShaderiv;
var_global PFNGLGETSHADERINFOLOGPROC   glGetShaderInfoLog;
var_global PFNGLGETPROGRAMIVPROC       glGetProgramiv;
var_global PFNGLGETPROGRAMINFOLOGPROC  glGetProgramInfoLog;

#define OGL4_Load_Proc(proc_) proc_ = ogl4_load_proc(Macro_Stringize(proc_))

fn_internal void *ogl4_load_proc(char *proc_name) {
  void *ptr = (void *)glXGetProcAddress((const GLubyte *)proc_name);
  if (!ptr) {
    // TODO(cmat): co_panic should be able to format strings? Or that's for logging only?
    log_fatal("failed to load OpenGL4 proc '%s'", proc_name);
    co_panic(str_lit("failed to load OpenGL4 proc"));
  }

  return ptr;
}

fn_internal void ogl4_load_api(void) {
  // NOTE(cmat): Load all OpenGL4 function pointers.
  OGL4_Load_Proc(glCreateBuffers);
  OGL4_Load_Proc(glBindBuffer);
  OGL4_Load_Proc(glBufferData);
  OGL4_Load_Proc(glBufferSubData);
  OGL4_Load_Proc(glCreateTextures);
  OGL4_Load_Proc(glCreateShader);
  OGL4_Load_Proc(glShaderSource);
  OGL4_Load_Proc(glCompileShader);
  OGL4_Load_Proc(glAttachShader);
  OGL4_Load_Proc(glCreateProgram);
  OGL4_Load_Proc(glLinkProgram);
  OGL4_Load_Proc(glDetachShader);
  OGL4_Load_Proc(glDeleteShader);
  OGL4_Load_Proc(glGetShaderiv);
  OGL4_Load_Proc(glGetShaderInfoLog);
  OGL4_Load_Proc(glGetProgramiv);
  OGL4_Load_Proc(glGetProgramInfoLog);
}



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

typedef struct OGL4_Buffer {
  GLuint buffer;
  U64    capacity;
} OGL4_Buffer;

typedef struct OGL4_Texture {
  GLuint texture;
  U32    width;
  U32    height;
} OGL4_Texture;

typedef struct OGL4_Shader {
  GLuint program;
} OGL4_Shader;

var_global struct {
  B32 initialized;
 
  U32 buffer_last_id;
  U32 shader_last_id;
  U32 sampler_last_id;
  U32 texture_last_id;
  U32 pipeline_last_id;

  OGL4_Buffer   buffers   [OGL4_Max_Buffers];
  OGL4_Texture  textures  [OGL4_Max_Textures];
  OGL4_Shader   shaders   [OGL4_Max_Shaders];
//  OGL4_Sampler  samplers  [OGL4_Max_Samplers];
//  OGL4_Pipeline pipelines [OGL4_Max_Pipelines];

} OGL4_State;

fn_internal R_Buffer r_buffer_allocate(U64 capacity, R_Buffer_Mode mode) {
  OGL4_Buffer *ogl4_buffer = &OGL4_State.buffers[OGL4_State.buffer_last_id++];
  glCreateBuffers(1, &ogl4_buffer->buffer);

  var_local_persist GLenum OGL4_Mode_Map[] = {
    GL_STATIC_DRAW,  // R_Buffer_Mode_Static
    GL_DYNAMIC_DRAW, // R_Buffer_Mode_Dynamic
  };

  glBindBuffer(GL_ARRAY_BUFFER, ogl4_buffer->buffer);
  glBufferData(GL_ARRAY_BUFFER, capacity, 0, OGL4_Mode_Map[mode]);

  R_Buffer result = {
    .id = OGL4_State.buffer_last_id
  };

  return result;
}

fn_internal void r_buffer_download(R_Buffer *buffer, U64 offset, U64 bytes, void *data) {
  OGL4_Buffer *ogl4_buffer = &OGL4_State.buffers[buffer->id - 1];
  glBindBuffer(GL_ARRAY_BUFFER, ogl4_buffer->buffer);
  glBufferSubData(GL_ARRAY_BUFFER, offset, bytes, data);
}

// TODO(cmat): Implement this.
fn_internal void r_buffer_destroy(R_Buffer *buffer) {
  Not_Implemented;
}

fn_internal R_Texture r_texture_allocate (R_Texture_Config *config) {
  OGL4_Texture *ogl4_texture = &OGL4_State.textures[OGL4_State.texture_last_id++];
  ogl4_texture->width   = config->width;
  ogl4_texture->height  = config->height;

  glCreateTextures(GL_TEXTURE_2D, 1, &ogl4_texture->texture);
  glBindTexture(GL_TEXTURE_2D, ogl4_texture->texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, config->width, config->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

  R_Texture result = { .id = OGL4_State.texture_last_id };
  return result;
}

fn_internal void r_texture_download (R_Texture *texture, U08 *texture_data) {
  OGL4_Texture *ogl4_texture = &OGL4_State.textures[texture->id - 1];
  glBindTexture(GL_TEXTURE_2D, ogl4_texture->texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ogl4_texture->width, ogl4_texture->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture_data);
}

fn_internal void r_texture_destroy  (R_Texture *texture) {
}


fn_internal R_Pipeline r_pipeline_create(R_Shader shader, R_Vertex_Format format) {
  R_Pipeline result = { };
  return result;
}

// TODO(cmat): Redundant.
fn_internal void ogl4_create_default_textures(void) {
  R_Texture_Config white_texture_conf = {
    .format = R_Texture_Format_RGBA_U08_Normalized,
    .width  = 2,
    .height = 2,
  };

  U32 white_texture_data[] = {
    0xFFAAFFFF, 0xFFFFFFFF,
    0xFFFFAAFF, 0xFFFFFFFF,
  };
  
  R_Texture_White = r_texture_allocate(&white_texture_conf);
  r_texture_download(&R_Texture_White, (U08 *)white_texture_data);
}

fn_internal GLuint ogl4_compile_shader(char *source, GLenum type) {
  GLuint shader = glCreateShader(type);

  const char *gl_source = (const char *)source;
  glShaderSource(shader, 1, &gl_source, 0);
  glCompileShader(shader);

  GLint log_length = 0;
  glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
  log_info("log length :: %d", log_length);
  if (log_length > 1) {
    Scratch scratch = { };
    Scratch_Scope(&scratch, 0) {
      char *log_buffer = arena_push_count(scratch.arena, char, log_length);
      glGetShaderInfoLog(shader, log_length, 0, log_buffer);
      log_fatal("OpenGL4: Shader compilation error.\n%s", log_buffer);
      log_info("Full shader source:\n%s\n", source);
    }

    co_panic(str_lit("OpenGL4: Failed to compile shader, can't continue."));
  }

  return shader;
}

fn_internal GLuint ogl4_create_program(char *vertex_source, char *fragment_source) {
  GLuint vertex_shader    = ogl4_compile_shader(vertex_source,   GL_VERTEX_SHADER);
  GLuint fragment_shader  = ogl4_compile_shader(fragment_source, GL_FRAGMENT_SHADER);

  GLuint program = glCreateProgram();
  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  glLinkProgram(program);

  GLint log_length = 0;
  glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
  log_info("log length :: %d", log_length);
  if (log_length > 1) {
    Scratch scratch = { };
    Scratch_Scope(&scratch, 0) {
      char *log_buffer = arena_push_count(scratch.arena, char, log_length);
      glGetProgramInfoLog(program, log_length, 0, log_buffer);
      log_fatal("OpenGL4: Program link error.\n%s", log_buffer);
    }

    co_panic(str_lit("OpenGL4: Failed to link program, can't continue."));
  }

  glDetachShader(program, vertex_shader);
  glDetachShader(program, fragment_shader);
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  return program;
}

fn_internal void ogl4_create_default_shaders(void) {
  OGL4_Shader *flat_2D_shader  = &OGL4_State.shaders[OGL4_State.shader_last_id++];
  flat_2D_shader->program      = ogl4_create_program(ogl4_shader_vertex_flat_2D, ogl4_shader_pixel_flat_2D);
  R_Shader_Flat_2D             = (R_Shader) { .id = OGL4_State.shader_last_id };

  OGL4_Shader *flat_3D_shader  = &OGL4_State.shaders[OGL4_State.shader_last_id++];
  flat_3D_shader->program      = ogl4_create_program(ogl4_shader_vertex_flat_3D, ogl4_shader_pixel_flat_3D);
  R_Shader_Flat_3D             = (R_Shader) { .id = OGL4_State.shader_last_id };

  OGL4_Shader *mtsdf_2D_shader = &OGL4_State.shaders[OGL4_State.shader_last_id++];
  mtsdf_2D_shader->program     = ogl4_create_program(ogl4_shader_vertex_mtsdf_2D, ogl4_shader_pixel_mtsdf_2D);
  R_Shader_MTSDF_2D            = (R_Shader) { .id = OGL4_State.shader_last_id };
}

fn_internal void r_init(PL_Render_Context *render_context) {
  ogl4_load_api();

  // NOTE(cmat): Create default resources.
  // ogl4_create_default_samplers();
  ogl4_create_default_shaders();
  ogl4_create_default_textures();
}

fn_internal void r_frame_flush(void) {
  glClearColor(.1f, .1f, .8f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT);
}


