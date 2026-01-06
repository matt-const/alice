// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

#if OS_MACOS
# include <Cocoa/Cocoa.h>

# include <QuartzCore/CAMetalLayer.h>
# include <CoreVideo/CoreVideo.h>
# include <Metal/Metal.h>

# include "pl_macos.m"

#elif OS_LINUX

# include <X11/Xlib.h>
# include <X11/Xatom.h>
# include <GL/gl.h>
# include <GL/glx.h>

# include "pl_linux.c"

#elif OS_WASM

# include "platform_wasm.c"

#else
# error "unsupported platform"
#endif
