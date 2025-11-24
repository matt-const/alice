compile_shader() {
  local name="$1"
  local file="${name}.hlsl"

  slangc "$file" -entry "${name}_vertex" -profile vs_5_0 -target metal -o "${name}_vertex.metal"
  slangc "$file" -entry "${name}_pixel"  -profile ps_5_0 -target metal -o "${name}_pixel.metal"

  xcrun -sdk macosx metal -o "${name}_vertex.ir" -c "${name}_vertex.metal"
  xcrun -sdk macosx metal -o "${name}_pixel.ir"  -c "${name}_pixel.metal"
}

compile_shader "flat_2D"
compile_shader "flat_3D"
compile_shader "mtsdf_2D"

xcrun -sdk macosx metallib -o baked_shaders.metallib *.ir

xxd -i baked_shaders.metallib | sed \
    -e "s/unsigned int baked_shaders_metallib_len/cb_global U64 metal_baked_shaders_bytes/g" \
    -e "s/unsigned char baked_shaders_metallib/cb_global U08 metal_baked_shaders_data/g" \
    > "render_shader_metal.gen.c"

rm *.metal
rm *.ir
rm *.metallib
