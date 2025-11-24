#version 450
layout(row_major) uniform;
layout(row_major) buffer;

#line 7 0
struct SLANG_ParameterGroup_Constant_Buffer_Viewport_2D_0
{
    mat4x4 NDC_From_Screen_0;
};


#line 7
layout(binding = 2)
layout(std140) uniform block_SLANG_ParameterGroup_Constant_Buffer_Viewport_2D_0
{
    mat4x4 NDC_From_Screen_0;
}Constant_Buffer_Viewport_2D_0;

#line 12394 1
layout(location = 0)
out vec4 entryPointParam_flat_2D_vertex_C_0;


#line 11 0
layout(location = 1)
out vec2 entryPointParam_flat_2D_vertex_U_0;


#line 11
flat layout(location = 2)
out uint entryPointParam_flat_2D_vertex_Texture_Slot_0;


#line 11
layout(location = 0)
in vec2 vertex_in_X_0;


#line 11
layout(location = 1)
in vec4 vertex_in_C_0;


#line 11
layout(location = 2)
in vec2 vertex_in_U_0;


#line 11
layout(location = 3)
in uint vertex_in_Texture_Slot_0;


#line 18
struct Pixel_In_0
{
    vec4 X_0;
    vec4 C_0;
    vec2 U_0;
    uint Texture_Slot_0;
};


#line 25
void main()
{

#line 26
    Pixel_In_0 pixel_out_0;
    pixel_out_0.X_0 = (((Constant_Buffer_Viewport_2D_0.NDC_From_Screen_0) * (vec4(vertex_in_X_0, 0.0, 1.0))));
    pixel_out_0.C_0 = vertex_in_C_0;
    pixel_out_0.U_0 = vertex_in_U_0;
    pixel_out_0.Texture_Slot_0 = vertex_in_Texture_Slot_0;
    Pixel_In_0 _S1 = pixel_out_0;

#line 31
    gl_Position = pixel_out_0.X_0;

#line 31
    entryPointParam_flat_2D_vertex_C_0 = _S1.C_0;

#line 31
    entryPointParam_flat_2D_vertex_U_0 = _S1.U_0;

#line 31
    entryPointParam_flat_2D_vertex_Texture_Slot_0 = _S1.Texture_Slot_0;

#line 31
    return;
}

