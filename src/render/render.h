// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

// ------------------------------------------------------------
// #-- Render Resources
#define R_Resource_None 0

typedef U32 R_Resource;

typedef R_Resource R_Shader;
typedef R_Resource R_Buffer;
typedef R_Resource R_Texture_2D;
typedef R_Resource R_Texture_3D;
typedef R_Resource R_Sampler;
typedef R_Resource R_Pipeline;

// NOTE(cmat): Invalid resource handles.

var_external R_Shader       R_Shader_Invalid;
var_external R_Buffer       R_Buffer_Invalid;
var_external R_Texture_2D   R_Texture_2D_Invalid;
var_external R_Texture_3D   R_Texture_2D_Invalid;
var_external R_Sampler      R_Sampler_Invalid;
var_external R_Pipeline     R_Pipeline_Invalid;

typedef U32 R_Buffer_Mode;
enum {
  R_Buffer_Mode_Static,
  R_Buffer_Mode_Dynamic,
};

typedef struct R_Buffer_Info {
  U64            capacity;
  R_Buffer_Mode  mode;
} R_Buffer_Info;

fn_internal R_Buffer        r_buffer_allocate (U64 capacity, R_Buffer_Mode mode);
fn_internal void            r_buffer_download (R_Buffer buffer, U64 offset, U64 bytes, void *data);
fn_internal R_Buffer_Info   r_buffer_info     (R_Buffer buffer);
fn_internal void            r_buffer_destroy  (R_Buffer *buffer);

typedef U32 R_Texture_Format;
enum {
  R_Texture_Format_RGBA_U08_Normalized,
  R_Texture_Format_RGBA_I08_Normalized,
  R_Texture_Format_R_U08_Normalized,
  R_Texture_Format_R_I08_Normalized,

  R_Texture_Format_F32,
};

typedef struct {
  R_Texture_Format format;
  U32              width;
  U32              height;
} R_Texture_Config;

fn_internal R_Texture_2D  r_texture_2D_allocate (R_Texture_Format format, U32 width, U32 height);
fn_internal void          r_texture_2D_download (R_Texture_2D texture, R_Texture_Format download_format, R2I region, void *data);
fn_internal void          r_texture_2D_destroy  (R_Texture_2D *texture);

fn_internal R_Texture_3D  r_texture_3D_allocate (R_Texture_Format format, U32 width, U32 height, U32 depth);
fn_internal void          r_texture_3D_download (R_Texture_3D texture, R_Texture_Format download_format, R3I region, void *data);
fn_internal void          r_texture_3D_destroy  (R_Texture_3D *texture);


typedef U32 R_Sampler_Filter;
enum {
  R_Sampler_Filter_Linear,
  R_Sampler_Filter_Nearest,
};

fn_internal R_Sampler r_sampler_create   (R_Sampler_Filter mag_filter, R_Sampler_Filter min_filter);
fn_internal void      r_sampler_destroy  (R_Sampler *sampler);

#define R_Vertex_Max_Attribute_Count 16
#define R_Declare_Vertex_Attribute(type_, x_) alignas(16) type_ x_

typedef U16 R_Vertex_Attribute_Format;
enum {
  R_Vertex_Attribute_Format_F32,
  R_Vertex_Attribute_Format_V2_F32,
  R_Vertex_Attribute_Format_V3_F32,
  R_Vertex_Attribute_Format_V4_F32,

  R_Vertex_Attribute_Format_U16,
  R_Vertex_Attribute_Format_V2_U16,
  R_Vertex_Attribute_Format_V3_U16,
  R_Vertex_Attribute_Format_V4_U16,

  R_Vertex_Attribute_Format_U32,
  R_Vertex_Attribute_Format_V2_U32,
  R_Vertex_Attribute_Format_V3_U32,
  R_Vertex_Attribute_Format_V4_U32,
  
  R_Vertex_Attribute_Format_V4_U08_Normalized
};

#pragma pack(push, 1)
typedef struct {
  U16                       offset;
  R_Vertex_Attribute_Format format;
} R_Vertex_Attribute;

typedef struct {
  U16                 stride;
  U16                 entry_count;
  R_Vertex_Attribute  entry_array[R_Vertex_Max_Attribute_Count];
} R_Vertex_Format;
#pragma pack(pop)


typedef U32 R_Binding_Type;
enum {
  R_Binding_Constant_Buffer,
  R_Binding_Storage_Buffer,
  R_Binding_Texture_2D,
  R_Binding_Texture_3D,
  R_Binding_Sampler
};

typedef U32 R_Binding_Stages;
enum {
  R_Binding_Stage_Vertex,
  R_Binding_Stage_Pixel,
};

typedef struct R_Shader_Binding {
  U32               binding_index;
  R_Binding_Type    type;
  U32               array_count;
  R_Binding_Stages  stages;
} R_Shader_Binding;

#define R_Shader_Max_Binding_Count 16

typedef struct R_Pipeline_Layout {
  U32              binding_count;
  R_Shader_Binding binding_array[R_Shader_Max_Binding_Count];
} R_Pipeline_Layout;

fn_internal R_Pipeline r_pipeline_create  (R_Shader shader, R_Vertex_Format *format, B32 depth_buffer);
fn_internal void       r_pipeline_destroy (R_Pipeline *pipeline);

typedef struct {
  R_Declare_Vertex_Attribute(V2F, X);
  R_Declare_Vertex_Attribute(V2F, U);
  R_Declare_Vertex_Attribute(U32, C);
} R_Vertex_XUC_2D;

var_global R_Vertex_Format R_Vertex_Format_XUC_2D = {
  .stride       = sizeof(R_Vertex_XUC_2D),
  .entry_count  = 3,
  .entry_array  = {
    { .offset   = offsetof(R_Vertex_XUC_2D, X), .format = R_Vertex_Attribute_Format_V2_F32            },
    { .offset   = offsetof(R_Vertex_XUC_2D, U), .format = R_Vertex_Attribute_Format_V2_F32            },
    { .offset   = offsetof(R_Vertex_XUC_2D, C), .format = R_Vertex_Attribute_Format_V4_U08_Normalized },
  },
};

typedef struct {
  R_Declare_Vertex_Attribute(V3F, X);
  R_Declare_Vertex_Attribute(V2F, U);
  R_Declare_Vertex_Attribute(U32, C);
} R_Vertex_XUC_3D;

var_global R_Vertex_Format R_Vertex_Format_XUC_3D = {
  .stride       = sizeof(R_Vertex_XUC_3D),
  .entry_count  = 3,
  .entry_array  = {
    { .offset   = offsetof(R_Vertex_XUC_3D, X), .format = R_Vertex_Attribute_Format_V3_F32            },
    { .offset   = offsetof(R_Vertex_XUC_3D, U), .format = R_Vertex_Attribute_Format_V2_F32            },
    { .offset   = offsetof(R_Vertex_XUC_3D, C), .format = R_Vertex_Attribute_Format_V4_U08_Normalized },
  },
};

// ------------------------------------------------------------
// #-- Render Commands

typedef U32 R_Command_Type;
enum {
  R_Command_Type_Draw,
};

typedef struct R_Command_Header {
  R_Command_Type type;
  U64 bytes;
  struct R_Command_Header *next;
} R_Command_Header;

typedef struct {
  Arena arena;
  R_Command_Header *first;
  R_Command_Header *last;
} R_Command_Buffer;

var_external R_Command_Buffer R_Commands;

#pragma pack(push, 1)

typedef struct R_Command_Draw {

  R_Buffer      constant_buffer;
  R_Buffer      vertex_buffer;
  R_Buffer      index_buffer;
  R_Pipeline    pipeline;
  R_Texture_2D  texture;
  R_Texture_3D  texture_volume;
  R_Sampler     sampler;

  U32           draw_index_count;
  U32           draw_index_offset;

  B32           depth_test;

  R2I           draw_region;
  R2I           clip_region;

} R_Command_Draw;

#pragma pack(pop)

// TODO(cmat): shouldn't be exposed in userland.
fn_internal void r_command_reset      (void);
fn_internal void r_command_push_draw  (R_Command_Draw *draw);

fn_internal void r_init               (PL_Render_Context *render_context);
fn_internal void r_frame_flush        (void);

// ------------------------------------------------------------
// #-- Default Resources

var_external R_Shader  R_Shader_Flat_2D;
var_external R_Shader  R_Shader_Flat_3D;
var_external R_Shader  R_Shader_Grid_3D;
var_external R_Shader  R_Shader_DVR_3D;
var_external R_Shader  R_Shader_SLI_3D;

var_external R_Texture_2D R_Texture_2D_White;
var_external R_Texture_3D R_Texture_3D_White;

var_external R_Sampler R_Sampler_Linear_Clamp;
var_external R_Sampler R_Sampler_Nearest_Clamp;

typedef struct {
  alignas(16) M4F NDC_From_Screen;
} R_Constant_Buffer_Viewport_2D;

typedef struct {
  alignas(16) M4F World_View_Projection;
  alignas(16) V3F Eye_Position;
} R_Constant_Buffer_World_3D;

