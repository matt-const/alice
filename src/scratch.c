// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)


fn_entry void callback(HTTP_Status status, void *user_data) {
  if (status == HTTP_Status_Download) {
  } else if (status == HTTP_Status_Allocate) {

    status->allocated_memory arena_push();
  }
}

HTTP_Request request = { };
http_request(str_lit("test.data"), callback, user_data);



// ------------------------------------------------------------
// #--

VERTEX_SHADER QUAD REGION: PARAM_SIZE + PARAM_DILATE * 2

#define PARAM_RADIUS 1.f
#define PARAM_DILATE 10.f
#define PARAM_SIZE   vec2(1, 1)

// from https://iquilezles.org/articles/distfunctions
float roundedBoxSDF(vec2 CenterPosition, vec2 Size, float Radius) {
    return length(max(abs(CenterPosition)-Size+Radius,0.0))-Radius;
}
void mainImage( out vec4 fragColor, in vec2 fragCoord ) {
    // The pixel space scale of the rectangle.
    vec2 size = vec2(PARAM_SIZE, PARAM_SIZE);
    
    // the pixel space location of the rectangle.
    vec2 location = vec2(0, 0);

    // How soft the edges should be (in pixels). Higher values could be used to simulate a drop shadow.
    float edgeSoftness  = 1.0f;
    
    // The radius of the corners (in pixels).
    float radius = PARAM_RADIUS;
    
    // Calculate distance to edge.   
    float distance 		= roundedBoxSDF(fragCoord.xy - location - (size/2.0f), size / 2.0f, radius);
    
    // Smooth the result (free antialiasing).
    float smoothedAlpha =  1.0f - smoothstep(0.0f, PARAM_DILATE * 2.0f,distance);
    
    vec4 quadColor		= vec4(smoothedAlpha);
    fragColor 			= quadColor;
}

///


shader_type spatial;

uniform int scale_0 : hint_range(1, 1024, 1);
uniform int scale_1 : hint_range(1, 1024, 1);

uniform float line_scale_0 : hint_range(0.001, 1, 0.001);
uniform float line_scale_1 : hint_range(0.001, 1, 0.001);

uniform vec4 color_0 : source_color;
uniform vec4 color_1 : source_color;


float pristineGrid( vec2 uv, vec2 lineWidth)
{
    vec2 ddx = dFdx(uv);
    vec2 ddy = dFdy(uv);
	
    vec2 uvDeriv = vec2(length(vec2(ddx.x, ddy.x)), length(vec2(ddx.y, ddy.y)));
    bvec2 invertLine = bvec2(lineWidth.x > 0.5, lineWidth.y > 0.5);
	
    vec2 targetWidth = vec2(
      invertLine.x ? 1.0 - lineWidth.x : lineWidth.x,
      invertLine.y ? 1.0 - lineWidth.y : lineWidth.y
      );
	
    vec2 drawWidth = clamp(targetWidth, uvDeriv, vec2(0.5));
    vec2 lineAA = uvDeriv * 1.5;
    vec2 gridUV = abs(fract(uv) * 2.0 - 1.0);
	
    gridUV.x = invertLine.x ? gridUV.x : 1.0 - gridUV.x;
    gridUV.y = invertLine.y ? gridUV.y : 1.0 - gridUV.y;
	
    vec2 grid2 = smoothstep(drawWidth + lineAA, drawWidth - lineAA, gridUV);

    grid2 *= clamp(targetWidth / drawWidth, 0.0, 1.0);
    grid2 = mix(grid2, targetWidth, clamp(uvDeriv * 2.0 - 1.0, 0.0, 1.0));
    grid2.x = invertLine.x ? 1.0 - grid2.x : grid2.x;
    grid2.y = invertLine.y ? 1.0 - grid2.y : grid2.y;
    return mix(grid2.x, 1.0, grid2.y);
}

void vertex() 
{
	//UV = VERTEX.xz;
}

void fragment() 
{
	vec3 grid_0 = vec3(pristineGrid(UV * float(scale_0), vec2(line_scale_0)));
	vec3 grid_1 = vec3(pristineGrid(UV * float(scale_1), vec2(line_scale_1)));
	
	vec3 grid_3 = mix(grid_1 * color_1.rgb, grid_0 * color_0.rgb, grid_0);
	
	ALBEDO =  grid_3;
}


// WGSL version of your Godot spatial shader

struct FragmentOutput {
    @location(0) color: vec4<f32>,
};

// Constants (replacing uniforms for quick testing)
const scale_0: i32 = 10;
const scale_1: i32 = 20;

const line_scale_0: f32 = 0.02;
const line_scale_1: f32 = 0.05;

const color_0: vec4<f32> = vec4<f32>(1.0, 0.0, 0.0, 1.0); // red
const color_1: vec4<f32> = vec4<f32>(0.0, 1.0, 0.0, 1.0); // green


// grid function from Best Darn Grid article
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

struct VertexIn {
  @location(0) pos: vec4f,
  @location(1) uv: vec2f,
}

struct VertexOut {
  @builtin(position) pos: vec4f,
  @location(0) uv: vec2f,
}

struct Camera {
  projection: mat4x4f,
  view: mat4x4f,
}
@group(0) @binding(0) var<uniform> camera: Camera;

struct GridArgs {
  lineColor: vec4f,
  baseColor: vec4f,
  lineWidth: vec2f,
}
@group(1) @binding(0) var<uniform> gridArgs: GridArgs;

@vertex
fn vertexMain(in: VertexIn) -> VertexOut {
  var out: VertexOut;
  out.pos = camera.projection * camera.view * in.pos;
  out.uv = in.uv;
  return out;
}

@fragment
fn fragmentMain(in: VertexOut) -> @location(0) vec4f {
  var grid = PristineGrid(in.uv, gridArgs.lineWidth);

  // lerp between base and line color
  return mix(gridArgs.baseColor, gridArgs.lineColor, grid * gridArgs.lineColor.a);
}
