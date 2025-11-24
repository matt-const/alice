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

  I32 visual_attributes[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
  XVisualInfo *visual_info = glXChooseVisual(display, screen, visual_attributes);

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

  GLXContext gl_context = glXCreateContext(display, visual_info, 0, GL_TRUE);
  glXMakeCurrent(display, window, gl_context);

  XFree(visual_info);
  visual_info = 0;

  Platform_Render_Context render_context = {
    .backend     = Platform_Render_Backend_OpenGL4,
    .os_handle_1 = 0,
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
  glXDestroyContext(display, gl_context);
  XDestroyWindow(display, window);
  XCloseDisplay(display);
}
