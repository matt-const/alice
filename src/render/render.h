// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

// ------------------------------------------------------------
// #-- Render Resources

typedef struct R_Resource { U64 id; } R_Resource;

typedef R_Resource R_Shader;
typedef R_Resource R_Buffer;
typedef R_Resource R_Texture;
typedef R_Resource R_Sampler;
typedef R_Resource R_Pipeline;

// NOTE(cmat): Invalid resource handles.

extern R_Shader    R_Shader_Invalid;
extern R_Buffer    R_Buffer_Invalid;
extern R_Texture   R_Texture_Invalid;
extern R_Sampler   R_Sampler_Invalid;
extern R_Pipeline  R_Pipeline_Invalid;

typedef U32 R_Buffer_Mode;
enum {
  R_Buffer_Mode_Static,
  R_Buffer_Mode_Dynamic,
};

typedef struct R_Buffer_Info {
  U64             capacity;
  R_Buffer_Mode   mode;
} R_Buffer_Info;

cb_function R_Buffer        r_buffer_allocate (U64 capacity, R_Buffer_Mode mode);
cb_function void            r_buffer_download (R_Buffer *buffer, U64 offset, U64 bytes, void *data);
cb_function void            r_buffer_destroy  (R_Buffer *buffer);
cb_function R_Buffer_Info   r_buffer_info     (R_Buffer *buffer);

typedef U32 R_Texture_Format;
enum {
  R_Texture_Format_RGBA_U08_Normalized,
  R_Texture_Format_RGBA_I08_Normalized,
  R_Texture_Format_R_U08_Normalized,
  R_Texture_Format_R_I08_Normalized,
};

typedef struct {
  R_Texture_Format  format;
  U32               width;
  U32               height;
} R_Texture_Config;

cb_function R_Texture r_texture_allocate (R_Texture_Config *config);
cb_function void      r_texture_download (R_Texture *texture, U08 *texture_data);
cb_function void      r_texture_destroy  (R_Texture *texture);

#define R_Vertex_Max_Attribute_Count 16
#define R_Declare_Vertex_Attribute(type, x) alignas(16) type x

typedef U08 R_Vertex_Attribute_Format;
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

typedef struct {
  U16 offset;
  R_Vertex_Attribute_Format format;
} R_Vertex_Attribute;

typedef struct {
  U16 stride;
  U16 entry_count;
  R_Vertex_Attribute entry_array[R_Vertex_Max_Attribute_Count];
} R_Vertex_Format;

cb_function R_Pipeline r_pipeline_create(R_Shader shader, R_Vertex_Format format);

typedef struct {
  R_Declare_Vertex_Attribute(V2F, X);
  R_Declare_Vertex_Attribute(U32, C);
  R_Declare_Vertex_Attribute(V2F, U);
  R_Declare_Vertex_Attribute(U32, Texture_Slot);
} R_Vertex_XCU_2D;

cb_global R_Vertex_Format R_Vertex_Format_XCU_2D = {
  .stride       = sizeof(R_Vertex_XCU_2D),
  .entry_count  = 4,
  .entry_array  = {
    { .offset   = offsetof(R_Vertex_XCU_2D, X),            .format = R_Vertex_Attribute_Format_V2_F32            },
    { .offset   = offsetof(R_Vertex_XCU_2D, C),            .format = R_Vertex_Attribute_Format_V4_U08_Normalized },
    { .offset   = offsetof(R_Vertex_XCU_2D, U),            .format = R_Vertex_Attribute_Format_V2_F32            },
    { .offset   = offsetof(R_Vertex_XCU_2D, Texture_Slot), .format = R_Vertex_Attribute_Format_U32               },
  },
};

typedef struct {
  R_Declare_Vertex_Attribute(V3F, X);
  R_Declare_Vertex_Attribute(U32, C);
  R_Declare_Vertex_Attribute(V2F, U);
  R_Declare_Vertex_Attribute(U32, Texture_Slot);
} R_Vertex_XCU_3D;

cb_global R_Vertex_Format R_Vertex_Format_XCU_3D = {
  .stride       = sizeof(R_Vertex_XCU_3D),
  .entry_count  = 4,
  .entry_array  = {
    { .offset   = offsetof(R_Vertex_XCU_3D, X),            .format = R_Vertex_Attribute_Format_V3_F32            },
    { .offset   = offsetof(R_Vertex_XCU_3D, C),            .format = R_Vertex_Attribute_Format_V4_U08_Normalized },
    { .offset   = offsetof(R_Vertex_XCU_3D, U),            .format = R_Vertex_Attribute_Format_V2_F32            },
    { .offset   = offsetof(R_Vertex_XCU_3D, Texture_Slot), .format = R_Vertex_Attribute_Format_U32               },
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

extern R_Command_Buffer R_Commands;

#define R_Texture_Slots         16
#define R_Max_Constant_Buffers  8

typedef struct {
  U32        index_count;
  U32        index_buffer_offset;
  U32        constant_buffer_count;
  B32        depth_testing;
  R_Buffer   vertex_buffer;
  R_Buffer   index_buffer;  
  R_Pipeline pipeline;
  R_Texture  texture_slots    [R_Texture_Slots];
  R_Sampler  sampler;
  R_Buffer   constant_buffers [R_Max_Constant_Buffers];
  R2I        clip_region;
  R2I        viewport_region;
} R_Command_Draw;

cb_function void r_command_reset  (void);
cb_function void r_command_draw   (R_Command_Draw *draw);

cb_function void r_init           (Platform_Render_Context *render_context);
cb_function void r_frame_flush    (void);

// ------------------------------------------------------------
// #-- Default Resources

extern R_Shader  R_Shader_Flat_2D;
extern R_Shader  R_Shader_Flat_3D;
extern R_Shader  R_Shader_MTSDF_2D;

extern R_Texture R_Texture_White;

extern R_Sampler R_Sampler_Linear_Clamp;
extern R_Sampler R_Sampler_Nearest_Clamp;

typedef struct {
  alignas(16) M4F NDC_From_Screen;
} R_Constant_Buffer_Viewport_2D;

typedef struct {
  alignas(16) M4F World_View_Projection;
  alignas(16) V3F Eye_Position;
} R_Constant_Buffer_World_3D;

