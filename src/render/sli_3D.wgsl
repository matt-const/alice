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

@group(0) @binding(3)
var Texture_Volume : texture_3d<f32>;

fn vec4_unpack_u32(packed: u32) -> vec4<f32> {
  let r = f32((packed >> 0)  & 0xFFu) / 255.0;
  let g = f32((packed >> 8)  & 0xFFu) / 255.0;
  let b = f32((packed >> 16) & 0xFFu) / 255.0;
  let a = f32((packed >> 24) & 0xFFu) / 255.0;

    return vec4<f32>(r, g, b, a);
}

struct VS_Out {
    @builtin(position)  X_Clip : vec4<f32>,
    @location(0)        X      : vec3<f32>,
    @location(1)        C      : vec4<f32>,
    @location(2)        U      : vec2<f32>,
};

@vertex
fn vs_main(@location(0) X : vec3<f32>,
           @location(1) U : vec2<f32>,
           @location(2) C : u32) -> VS_Out {

   var out : VS_Out;

   out.X_Clip = transpose(World_3D.World_View_Projection) * vec4<f32>(X, 1.0);
   out.X      = X;
   out.C      = vec4_unpack_u32(C);
   out.U      = U;

   return out;
}

const box_min       = vec3<f32>(-1.0, -1.0, -1.0);
const box_max       = vec3<f32>( 1.0,  1.0,  1.0);

fn sample_volume(position: vec3<f32>) -> f32 {
    let p = clamp(
        (position - box_min) / (box_max - box_min),
        vec3<f32>(0.0),
        vec3<f32>(1.0)
    );

    let size = vec3<f32>(textureDimensions(Texture_Volume));
    let coord = p * (size - 1.0);

    let base = vec3<i32>(floor(coord));
    let frac = fract(coord);

    let c000 = textureLoad(Texture_Volume, base + vec3<i32>(0,0,0), 0).r;
    let c100 = textureLoad(Texture_Volume, base + vec3<i32>(1,0,0), 0).r;
    let c010 = textureLoad(Texture_Volume, base + vec3<i32>(0,1,0), 0).r;
    let c110 = textureLoad(Texture_Volume, base + vec3<i32>(1,1,0), 0).r;
    let c001 = textureLoad(Texture_Volume, base + vec3<i32>(0,0,1), 0).r;
    let c101 = textureLoad(Texture_Volume, base + vec3<i32>(1,0,1), 0).r;
    let c011 = textureLoad(Texture_Volume, base + vec3<i32>(0,1,1), 0).r;
    let c111 = textureLoad(Texture_Volume, base + vec3<i32>(1,1,1), 0).r;

    let c00 = mix(c000, c100, frac.x);
    let c10 = mix(c010, c110, frac.x);
    let c01 = mix(c001, c101, frac.x);
    let c11 = mix(c011, c111, frac.x);

    let c0 = mix(c00, c10, frac.y);
    let c1 = mix(c01, c11, frac.y);

    return mix(c0, c1, frac.z);
}

@fragment
fn fs_main(@location(0) X : vec3<f32>,
           @location(1) C : vec4<f32>,
           @location(2) U : vec2<f32> ) -> @location(0) vec4<f32> {

  let sample = sample_volume(X);
  let color  = textureSample(Texture, Sampler, vec2<f32>(sample, 0));
  return color;
}
