  compile_shader() {
  local name="$1"
  local file="${name}.hlsl"

  slangc "$file" -entry "${name}_vertex" -profile vs_5_0 -target wgsl -o "${name}_vertex.wgsl"
  slangc "$file" -entry "${name}_pixel"  -profile ps_5_0 -target wgsl -o "${name}_pixel.wgsl"

  echo "cb_global char * ogl4_shader_vertex_${name} = " >> "render_shader_webgpu.gen.c"
  sed 's/.*/  "&\\n"/' "${name}_vertex.glsl"            >> "render_shader_webgpu.gen.c"
  echo ";"                                              >> "render_shader_webgpu.gen.c"

  echo "cb_global char * ogl4_shader_pixel_${name} = "  >> "render_shader_webgpu.gen.c"
  sed 's/.*/  "&\\n"/' "${name}_pixel.glsl"             >> "render_shader_webgpu.gen.c"
  echo ";"                                              >> "render_shader_webgpu.gen.c"
}

rm render_shader_webgpu.gen.c

compile_shader "flat_2D"
compile_shader "flat_3D"
compile_shader "mtsdf_2D"

rm *.glsl
