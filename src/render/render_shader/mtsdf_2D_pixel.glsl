#version 450
layout(row_major) uniform;
layout(row_major) buffer;

#line 4 0
layout(binding = 0)
uniform texture2D  Texture_Array_0[16];


#line 5
layout(binding = 1)
uniform sampler Sampler_0;


#line 34
float msdf_median_0(float r_0, float g_0, float b_0)
{

#line 35
    return max(min(r_0, g_0), min(max(r_0, g_0), b_0));
}


#line 13393 1
float saturate_0(float x_0)
{

#line 13401
    return clamp(x_0, 0.0, 1.0);
}


#line 13401
layout(location = 0)
out vec4 entryPointParam_mtsdf_2D_pixel_0;


#line 13401
layout(location = 0)
in vec4 pixel_in_C_0;


#line 13401
layout(location = 1)
in vec2 pixel_in_U_0;


#line 13401
flat layout(location = 2)
in uint pixel_in_Texture_Slot_0;


#line 38 0
void main()
{

#line 39
    vec3 sample_0 = (texture(sampler2D(Texture_Array_0[pixel_in_Texture_Slot_0],Sampler_0), (pixel_in_U_0))).xyz;

#line 39
    entryPointParam_mtsdf_2D_pixel_0 = vec4(pixel_in_C_0.xyz, pixel_in_C_0.w * saturate_0(1.5 * (msdf_median_0(sample_0.x, sample_0.y, sample_0.z) - 0.5) + 0.5));

#line 39
    return;
}

