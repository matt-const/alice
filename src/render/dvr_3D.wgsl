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
  let g = f32((packed >> 8)  &0xFFu) / 255.0;
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

const box_min       = vec3<f32>(-1.0);
const box_max       = vec3<f32>( 1.0);
const ray_steps     = 256;
const ray_step_size = 0.01;

// const ray_steps     = 256;
// const ray_step_size = 0.01;

fn intersect_ray_box(ray_origin : vec3<f32>, ray_direction : vec3<f32>) -> vec2<f32> {
  let inv_direction = 1.f / ray_direction;

  let t0      = (box_min - ray_origin) * inv_direction;
  let t1      = (box_max - ray_origin) * inv_direction;

  let t_min   = min(t0, t1);
  let t_max   = max(t0, t1);

  let t_enter = max(max(t_min.x, t_min.y), t_min.z);
  let t_exit  = min(min(t_max.x, t_max.y), t_max.z);

  return vec2<f32>(t_enter, t_exit);
}

fn sample_volume(p : vec3<f32>) -> f32 {
  let t = vec2<f32>(0.6f, 0.25);
  let q = vec2<f32>(length(p.xz) - t.x, p.y);
  let r = (length(q) - t.y);

  if (r <= 0.0) {
    return 1.0;
  } else {
    return r;
  }
}

fn transfer_function(value : f32) -> vec4<f32> {
  let alpha = 1.0 - exp(-value * ray_step_size);
  let color = textureSample(Texture, Sampler, vec2<f32>(value, 0));
  return vec4<f32>(color.rgb, alpha);
}

@fragment
fn fs_main(@location(0) X : vec3<f32>,
           @location(1) C : vec4<f32>,
           @location(2) U : vec2<f32> ) -> @location(0) vec4<f32> {

  let ray_origin    = World_3D.Eye_Position;
  let ray_direction = normalize(X - ray_origin);
  let t_hit         = intersect_ray_box(ray_origin, ray_direction);
  let t_enter     = t_hit.x;
  let t_exit      = t_hit.y;

  if (t_exit < t_enter || t_exit < 0.0) {
    discard;
  }

  var accum_color = vec4<f32>(0.0);
  var ray_t       = max(t_enter, 0.0);

  for (var it = 0; it < ray_steps; it++) {
  
    // NOTE(cmat): If dynamic loops supported.
    // if ((ray_t > t_exit) || accum_color.a >= 1.0) { break; }
    let mask = select(1.0, 0.0, (ray_t > t_exit) || accum_color.a >= 1.0);

    let sample_position = ray_origin + ray_t * ray_direction;
    let value           = sample_volume(sample_position);
    let color           = transfer_function(value);
    accum_color        += mask * (1.0 - accum_color.a) * vec4<f32>(color.rgb * color.a, color.a);
    ray_t              += ray_step_size;
  }

  return accum_color;
}
