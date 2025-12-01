// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

#include "render.c"

#if OS_MACOS
# include "render_shader/render_shader_metal.gen.c"

# include "render_metal.m"

#elif OS_LINUX
# include "render_shader/render_shader_opengl4.gen.c"

# include "render_opengl_corearb.c"
# include "render_opengl4.c"

#endif
