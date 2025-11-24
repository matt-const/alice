#!/bin/bash
# (C) Copyright 2025 Matyas Constans
# Licensed under the MIT License (https://opensource.org/license/mit/)

if [[ "$(uname)" == "Linux" ]]; then
 

  # ------------------------------------------------------------
  # #-- Compile Shaders
  
  pushd src/render/render_shader/ > /dev/null
  ./compile_opengl4.sh
  popd > /dev/null

  # ------------------------------------------------------------
  # #-- Compile Program

  echo "compiling program..."
  mkdir -p build
  pushd build > /dev/null
  code_path=$(realpath "../src/")
 
  # NOTE(cmat): Compiler arguments.
  compiler="
  -O0 -g -fsanitize=address -fsanitize=undefined -fsanitize-address-use-after-scope
  -fno-omit-frame-pointer
  -I${code_path}
  "

  # NOTE(cmat): Linker arguments.
  libraries="-o test
  -lGL
  -lX11
  -lXrandr
  "

  # NOTE(cmat): Build flags.
  defines="
  -DBUILD_DEBUG=1
  -DBUILD_ASSERT=1
  "

  # NOTE(cmat): Translation units.
  source="
  ${code_path}/entry.c
  "

  # NOTE(cmat): Compile & Link
  clang $compiler $defines $source $libraries 

  popd > /dev/null

elif [[ "$(uname)" == "Darwin" ]]; then

  # ------------------------------------------------------------
  # #-- Compile Shaders

  echo "compiling shaders..."
  pushd src/render/render_shader/ > /dev/null
  ./compile_metal.sh
  popd > /dev/null

  # ------------------------------------------------------------
  # #-- Compile Program

  echo "compiling program..."
  mkdir -p build
  pushd build > /dev/null
  code_path=$(realpath "../src/")

  # NOTE(cmat): Compiler arguments.
  compiler="
  -O0 -g -x objective-c
  -fsanitize=address -fsanitize=undefined -fsanitize-address-use-after-scope
  -fno-omit-frame-pointer
  -I${code_path}
  -I${code_path}/thirdparty/freetype/include
  -L${code_path}/thirdparty/freetype/lib/macos
  "

  # NOTE(cmat): Linker arguments.
  libraries="
      -framework Foundation
      -framework AppKit
      -framework QuartzCore
      -framework Metal
      -lfreetype
      -o test"

  # NOTE(cmat): Build flags.
  defines="
  -DBUILD_DEBUG=1
  -DBUILD_ASSERT=1
  "

  # NOTE(cmat): Translation units.
  source="
  ${code_path}/entry.c
  "

  # NOTE(cmat): Compile & Link
  clang $compiler $defines $source $libraries 

  popd > /dev/null

fi

