// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

Texture2D    Texture_Array[16];
SamplerState Sampler;

cbuffer Constant_Buffer_Viewport_2D  {
  float4x4 NDC_From_Screen;
};

struct Vertex_In {
  float2 X          : POSITION;
  float4 C          : COLOR;
  float2 U          : TEXCOORD0;
  uint Texture_Slot : TEXCOORD1;
};

struct Pixel_In {
  float4 X                          : SV_POSITION;
  float4 C                          : COLOR;
  float2 U                          : TEXCOORD0;
  nointerpolation uint Texture_Slot : TEXCOORD1;
};

Pixel_In flat_2D_vertex(Vertex_In vertex_in) {
  Pixel_In pixel_out;
  pixel_out.X            = mul(float4(vertex_in.X, 0, 1), NDC_From_Screen);
  pixel_out.C            = vertex_in.C;
  pixel_out.U            = vertex_in.U;
  pixel_out.Texture_Slot = vertex_in.Texture_Slot;
  return pixel_out;
}

float4 flat_2D_pixel(Pixel_In pixel_in) {
  return pixel_in.C * Texture_Array[pixel_in.Texture_Slot].Sample(Sampler, pixel_in.U);
}

