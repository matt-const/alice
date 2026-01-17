@group(0) @binding(0)
var Texture : texture_2d<f32>;

@group(0) @binding(1)
var Sampler : sampler;

struct World_3D_Type {
  @align(16) World_View_Projection : mat4x4<f32>,
  @align(16) Eye_Position          : vec3<f32>,
};

@group(0) @binding(2)
var<uniform> World_3D : World_3D_Type;

fn vec4_unpack_u32(packed: u32) -> vec4<f32> {
  let r = f32((packed >> 0)  & 0xFFu) / 255.0;
  let g = f32((packed >> 8)  & 0xFFu) / 255.0;
  let b = f32((packed >> 16) & 0xFFu) / 255.0;
  let a = f32((packed >> 24) & 0xFFu) / 255.0;

    return vec4<f32>(r, g, b, a);
}

@group(0) @binding(3)
var Texture_Volume : texture_3d<f32>;

struct VS_Out {
    @builtin(position)  X : vec4<f32>,
    @location(0)        C : vec4<f32>,
    @location(1)        U : vec2<f32>,
};

@vertex
fn vs_main(@location(0) X : vec3<f32>,
           @location(1) U : vec2<f32>,
           @location(2) C : u32) -> VS_Out {

   var out : VS_Out;

   out.X = transpose(World_3D.World_View_Projection) * vec4<f32>(X, 1.0);
   out.C = vec4_unpack_u32(C);
   out.U = U;

   return out;
}

// TODO(cmat): Temporary, based on https://github.com/toji/pristine-grid-webgpu.
// TODO(cmat): Reimplement based on the original article, add major/minor grid rendering.
fn PristineGrid(uv: vec2f, lineWidth: vec2f) -> f32 {
    let uvDDXY = vec4f(dpdx(uv), dpdy(uv));
    let uvDeriv = vec2f(length(uvDDXY.xz), length(uvDDXY.yw));
    let invertLine: vec2<bool> = lineWidth > vec2f(0.5);
    let targetWidth: vec2f = select(lineWidth, 1 - lineWidth, invertLine);
    let drawWidth: vec2f = clamp(targetWidth, uvDeriv, vec2f(0.5));
    let lineAA: vec2f = uvDeriv * 1.5;
    var gridUV: vec2f = abs(fract(uv) * 2.0 - 1.0);
    gridUV = select(1 - gridUV, gridUV, invertLine);
    var grid2: vec2f = smoothstep(drawWidth + lineAA, drawWidth - lineAA, gridUV);
    grid2 *= saturate(targetWidth / drawWidth);
    grid2 = mix(grid2, targetWidth, saturate(uvDeriv * 2.0 - 1.0));
    grid2 = select(grid2, 1.0 - grid2, invertLine);
    return mix(grid2.x, 1.0, grid2.y);
}

const line_width: vec2<f32> = vec2<f32>(0.01, 0.01);

@fragment
fn fs_main(@location(0) C : vec4<f32>,
           @location(1) U : vec2<f32> ) -> @location(0) vec4<f32> {

  let grid  = PristineGrid(U, line_width);
  let color = mix(vec4<f32>(0.0, 0.0, 0.0, 0.0), vec4<f32>(1.0, 1.0, 1.0, 1.0), grid);
  return color;
}
