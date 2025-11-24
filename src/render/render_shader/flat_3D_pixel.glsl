#version 450
layout(row_major) uniform;
layout(row_major) buffer;

#line 4 0
layout(binding = 0)
uniform texture2D  Texture_Array_0[16];


#line 5
layout(binding = 1)
uniform sampler Sampler_0;


#line 2080 1
layout(location = 0)
out vec4 entryPointParam_flat_3D_pixel_0;


#line 2080
layout(location = 0)
in vec4 pixel_in_C_0;


#line 2080
layout(location = 1)
in vec2 pixel_in_U_0;


#line 2080
flat layout(location = 2)
in uint pixel_in_Texture_Slot_0;


#line 35 0
void main()
{

#line 35
    entryPointParam_flat_3D_pixel_0 = pixel_in_C_0 * (texture(sampler2D(Texture_Array_0[pixel_in_Texture_Slot_0],Sampler_0), (pixel_in_U_0)));

#line 35
    return;
}

