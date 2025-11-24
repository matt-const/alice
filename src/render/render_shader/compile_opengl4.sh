compile_shader() {
  local name="$1"
  local file="${name}.hlsl"

  slangc "$file" -entry "${name}_vertex" -profile vs_5_0 -target glsl -o "${name}_vertex.glsl"
  slangc "$file" -entry "${name}_pixel"  -profile ps_5_0 -target glsl -o "${name}_pixel.glsl"
}

compile_shader "flat_2D"
compile_shader "flat_3D"
compile_shader "mtsdf_2D"

