// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

cb_global Platform_Frame_State linux_frame_state;

cb_function Platform_Bootstrap linux_default_bootstrap(void) {
  Platform_Bootstrap boot = {
    .title        = str_lit("Alice Engine"),
    .next_frame   = 0,
    .render       = {
      .resolution = v2i(2560, 1440),
    },
  };

  return boot;
}

cb_function Platform_Frame_State *platform_frame_state(void) {
  return &linux_frame_state;
} 

cb_function void base_entry_point(Array_Str command_line) {
  Platform_Bootstrap boot = linux_default_bootstrap();
  platform_entry_point(command_line, &boot);

  Display *display = XOpenDisplay(0);
  I32 screen = DefaultScreen(display);

  int framebuffer_attribs[] = {
    GLX_X_RENDERABLE  , True           ,
    GLX_DRAWABLE_TYPE , GLX_WINDOW_BIT ,
    GLX_RENDER_TYPE   , GLX_RGBA_BIT   ,
    GLX_X_VISUAL_TYPE , GLX_TRUE_COLOR ,
    GLX_RED_SIZE      , 8              ,
    GLX_GREEN_SIZE    , 8              ,
    GLX_BLUE_SIZE     , 8              ,
    GLX_ALPHA_SIZE    , 8              ,
    GLX_DEPTH_SIZE    , 24             ,
    GLX_DOUBLEBUFFER  , True           ,
    None
  };

  int framebuffer_count             = 0;
  GLXFBConfig *framebuffer_configs  = glXChooseFBConfig(display, screen, framebuffer_attribs, &framebuffer_count);
  XVisualInfo *visual_info          = glXGetVisualFromFBConfig(display, framebuffer_configs[0]);

  Colormap color_map = XCreateColormap(display, RootWindow(display, visual_info->screen), visual_info->visual, AllocNone);
  XSetWindowAttributes window_attributes = {
    .colormap = color_map,
    .event_mask = ExposureMask | KeyPressMask | StructureNotifyMask,
  };

  Window window = XCreateWindow(
      display,
      RootWindow(display, visual_info->screen),
      0, // NOTE(cmat): X-position
      0, // NOTE(cmat): Y-position
      boot.render.resolution.x, // NOTE(cmat): Width
      boot.render.resolution.y, // NOTE(cmat): Height
      0,
      visual_info->depth,
      InputOutput,
      visual_info->visual,
      CWColormap | CWEventMask,
      &window_attributes);

  XStoreName(display, window, "Alice Engine");

  // NOTE(cmat): Handle window on close.
  Atom window_message_delete = XInternAtom(display, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(display, window, &window_message_delete, 1);
  XMapWindow(display, window);

  B32 running = 1;
  B32 first_frame = 1;
  XEvent event = { };

  typedef GLXContext (*PFNGLXCREATECONTEXTATTRIBSARBPROC) (Display *, GLXFBConfig, GLXContext, Bool, const int*);
  PFNGLXCREATECONTEXTATTRIBSARBPROC glXCreateContextAttribsARB =
    (PFNGLXCREATECONTEXTATTRIBSARBPROC)glXGetProcAddressARB((const GLubyte *)"glXCreateContextAttribsARB");

  int context_attribs[] = {
    GLX_CONTEXT_MAJOR_VERSION_ARB , 4                                      ,
    GLX_CONTEXT_MINOR_VERSION_ARB , 5                                      ,
    GLX_CONTEXT_PROFILE_MASK_ARB  , GLX_CONTEXT_CORE_PROFILE_BIT_ARB       ,
    GLX_CONTEXT_FLAGS_ARB         , GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB ,
    None
  };

  GLXContext opengl_context = glXCreateContextAttribsARB(display, framebuffer_configs[0], 0, True, context_attribs);
  if (!opengl_context) {
    core_panic(str_lit("failed to create an OpenGL 4.5 context"));
  }

  glXMakeCurrent(display, window, opengl_context);
  XFree(framebuffer_configs);
  XFree(visual_info);
  visual_info = 0;

  Platform_Render_Context render_context = {
    .backend     = Platform_Render_Backend_OpenGL4,
    .os_handle_1 = (U08 *)&opengl_context,
    .os_handle_2 = 0,
  };

  while (running) {

    // NOTE(cmat): Poll events.
    while (XPending(display)) {
      XNextEvent(display, &event);

      switch (event.type) {
        case KeyPress: {
        } break;

        case ClientMessage: {
          if ((Atom)event.xclient.data.l[0] == window_message_delete) {
            running = 0;
          }

        } break;
        
        case ConfigureNotify: {
          I32 window_width  = event.xconfigure.width;
          I32 window_height = event.xconfigure.height;

          linux_frame_state.display.resolution.x = (F32)window_width;
          linux_frame_state.display.resolution.y = (F32)window_height;
        } break;
      }
    }

    linux_frame_state.display.frame_index += 1;
    linux_frame_state.display.frame_delta = 1.f / 200.f;

    // glClearColor(.1f, .8f, .1f, 1.f);
    // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // NOTE(cmat) Call into user-code.
    boot.next_frame(first_frame, &render_context);
    first_frame = 0;

    glXSwapBuffers(display, window);
  }

  // NOTE(cmat): Cleanup.
  glXMakeCurrent(display, None, 0);
  glXDestroyContext(display, opengl_context);
  XDestroyWindow(display, window);
  XCloseDisplay(display);
}
