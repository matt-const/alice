// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

// ------------------------------------------------------------
// #-- Global State
volatile cb_global  B32                   MacOS_Exit_Application = 0;
volatile cb_global  B32                   MacOS_Frame_Started    = 0;
cb_global           Platform_Frame_State  MacOS_Frame_State      = { };

typedef struct MacOS_Render_Thread_Data {
  id<MTLDevice>              metal_device;
  CAMetalLayer              *metal_layer;
  Platform_Next_Frame_Hook  *next_frame;
} MacOS_Render_Thread_Data;

// ------------------------------------------------------------
// #-- MacOS View
@interface MacOS_View : NSView
@end

@implementation MacOS_View
@end

// ------------------------------------------------------------
// #-- Entry Point

cb_function Platform_Bootstrap macos_default_bootstrap(void) {
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
  Assert(atomic_read_i32(&MacOS_Frame_Started) == 1, "calling platform_frame_state before any frame has started");
  return &MacOS_Frame_State;
}

// ------------------------------------------------------------
// #-- Frame State

cb_function void macos_update_input(Platform_Input *input) {
  // NOTE(cmat): Mouse position.
  NSWindow *window       = [NSApp keyWindow];
  NSView   *view         = [window contentView];
  CGFloat   scale        = [window backingScaleFactor];
  NSPoint   global_mouse = [NSEvent mouseLocation];
  NSPoint   window_mouse = [window convertPointFromScreen:global_mouse];
  NSPoint   local_mouse  = [view convertPoint:window_mouse fromView:nil];

  V2F new_position            = v2f(scale * (F32)local_mouse.x, scale * (F32)local_mouse.y);
  input->mouse.position_dt    = v2f_sub(new_position, input->mouse.position);
  input->mouse.position       = new_position;

  // NOTE(cmat): Mouse buttons.
  NSUInteger mouse_buttons = [NSEvent pressedMouseButtons];
  input->mouse.left.down_first_frame    = (mouse_buttons & (1 << 0)) && !input->mouse.left.down;
  input->mouse.left.down                = (mouse_buttons & (1 << 0));
  input->mouse.right.down_first_frame   = (mouse_buttons & (1 << 1)) && !input->mouse.right.down;
  input->mouse.right.down               = (mouse_buttons & (1 << 1));
  input->mouse.middle.down_first_frame  = (mouse_buttons & (1 << 2)) && !input->mouse.middle.down;
  input->mouse.middle.down              = (mouse_buttons & (1 << 2));
}

cb_function void macos_update_frame_state(Platform_Frame_State *state, MacOS_Render_Thread_Data *macos_render_context) {
  state->display.frame_index += 1;
  state->display.frame_delta = 1.f / 60.f;
  state->display.resolution = (V2F) {
    .x = (F32)macos_render_context->metal_layer.drawableSize.width,
    .y = (F32)macos_render_context->metal_layer.drawableSize.height,
  };

  macos_update_input(&state->input);
}

// ------------------------------------------------------------
// #-- Render Thread

cb_function CVReturn macos_display_link_hook( CVDisplayLinkRef    display_link,
                                              const CVTimeStamp  *in_now,
                                              const CVTimeStamp  *in_output_time,
                                              CVOptionFlags       flags_in,
                                              CVOptionFlags      *flags_out,
                                              void               *user_data) {

  B32                  first_frame       = 0;
  cb_local_persist B32 initialize_thread = 0;
  if (!initialize_thread) {
    initialize_thread = 1;
    first_frame       = 1;

    scratch_init_for_thread();
    atomic_write_i32(&MacOS_Frame_Started, 1);
  }

  B32 quit = atomic_read_i32(&MacOS_Exit_Application);
  if (!quit) {
      MacOS_Render_Thread_Data *macos_render_context = (MacOS_Render_Thread_Data *)user_data;

      Platform_Render_Context render_context = {
        .backend      = Platform_Render_Backend_Metal,
        .metal_device = (U08 *)macos_render_context->metal_device,
        .metal_layer  = (U08 *)macos_render_context->metal_layer,
      };

      @autoreleasepool {
        macos_update_frame_state(&MacOS_Frame_State, macos_render_context);
        macos_render_context->next_frame(first_frame, &render_context);
      }
  } else {
    dispatch_async(dispatch_get_main_queue(), ^{ [NSApp terminate:nil]; });
  }
  
  return kCVReturnSuccess;
}


cb_function void base_entry_point(Array_Str command_line) {
  Platform_Bootstrap boot = macos_default_bootstrap();
  platform_entry_point(command_line, &boot);
    
  // NOTE(cmat): Use the bootstrap configuration to setup
  // the window render event loop. If next_frame is 0, we
  // just start the program as a non-graphical program -- 
  // we exit the platform layer and give back control to
  // the core layer.
  if (boot.next_frame) {
    NSApplication *ns_app = [NSApplication sharedApplication];
    [ns_app setActivationPolicy:NSApplicationActivationPolicyRegular];

    NSRect ns_rect = NSMakeRect(0, 0, boot.render.resolution.x,  boot.render.resolution.y);

    // NOTE(cmat): Window setup.
    NSWindow *ns_window = [[NSWindow alloc] initWithContentRect:ns_rect
                                            styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable)
                                            backing:NSBackingStoreBuffered
                                            defer:NO];

    [ns_window setTitle:@"test"];
    [ns_window makeKeyAndOrderFront:nil];
    [ns_window center];

    // NOTE(cmat): Handle window closing as an event.
    [[NSNotificationCenter defaultCenter] addObserverForName:NSWindowWillCloseNotification
                                              object:ns_window
                                              queue:nil
                                              usingBlock:^(NSNotification *note) { MacOS_Exit_Application = 1; } ];

    NSView *ns_view = (NSView *)[[MacOS_View alloc] initWithFrame:ns_rect];
    [ns_window setContentView:ns_view];

    // NOTE(cmat): Create metal device. Unfortunately, this must be done on the main-thread.
    id<MTLDevice> metal_device = 0;
     metal_device = MTLCreateSystemDefaultDevice();

    // NOTE(cmat): Create metal layer. This must be done on the main-thread as well.
    CAMetalLayer *metal_layer = 0;      
    metal_layer               = [CAMetalLayer layer];
    metal_layer.device        = metal_device;
    metal_layer.pixelFormat   = MTLPixelFormatBGRA8Unorm;
    metal_layer.frame         = ns_view.bounds;
    metal_layer.contentsScale = [ns_view.window backingScaleFactor];

    // NOTE(cmat): Associcate metal layer with our NSView.
    [ns_view setLayer:metal_layer];
    [ns_view setWantsLayer:YES];
    [ns_view setPostsFrameChangedNotifications:YES];
    [ns_view.window makeFirstResponder:ns_view];

    // NOTE(cmat): Add an observer that dynamically resizes the metal view.
    [[NSNotificationCenter defaultCenter] addObserverForName:NSViewFrameDidChangeNotification object:ns_view queue:nil usingBlock:^(NSNotification *note) {
        CGSize new_size           = ns_view.bounds.size;
        CGFloat scale             = [ns_view.window backingScaleFactor];
        metal_layer.frame         = ns_view.bounds;
        metal_layer.drawableSize  = CGSizeMake(new_size.width * scale, new_size.height * scale);         
    }];

    MacOS_Render_Thread_Data render_thread_data = {
      .metal_device = metal_device,
      .metal_layer  = metal_layer,
      .next_frame   = boot.next_frame,
    };

    // TODO(cmat): This is deprecated, but can't really find the proper alternative...
    CVDisplayLinkRef display_link;
    CVDisplayLinkCreateWithActiveCGDisplays(&display_link);
    CVDisplayLinkSetOutputCallback(display_link, &macos_display_link_hook, &render_thread_data);
    CVDisplayLinkStart(display_link);

    [ns_app activateIgnoringOtherApps:YES];
    [ns_app run];

    CVDisplayLinkStop(display_link);
    CVDisplayLinkRelease(display_link);
  }
}
