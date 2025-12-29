const wasm_context = {
  canvas:           null,
  memory:           null,
  export_table:     null,
  webgpu:           null,
  frame_time_last:  null,

  frame_state: {
    display: {
      frame_delta: 0,
      resolution: { width: 0, height: 0 },
    },

    input: {
      mouse: {
        position: { x: 0, y: 0, },
        button: { left: 0, right: 0, middle: 0, },
      }
    }
  },

  shared_memory: {
    frame_state: null,
  },

  // TODO(cmat): Temporary.
  webgpu_pass_encoder: null,
};

function js_string_from_c_string(string_len, string_txt) {
  let c_string = new Uint8Array(wasm_context.memory.buffer, string_txt, string_len);
  let js_string = '';
  for (let it = 0; it < string_len; it++) {
    js_string += String.fromCharCode(c_string[it]);
  }

  return js_string;
}

// ------------------------------------------------------------
// #-- NOTE(cmat): JS - WASM core API.

function js_core_unix_time() {
  const date = new Date();
  const local_offset = date.getTimezoneOffset() * 60 * 1000;
  return Date.now() - local_offset;
}

function js_core_stream_write(stream_mode, string_len, string_txt) {
  const js_string = js_string_from_c_string(string_len, string_txt);

  if (stream_mode == 1) {
    console.log(js_string);
  } else if (stream_mode == 2) {
    console.error(js_string);
  }
}

function js_core_panic(string_len, string_txt) {
  const js_string = js_string_from_c_string(string_len, string_txt);
  alert(js_string);
  throw "PANIC ## " + js_string;
}

// ------------------------------------------------------------
// #-- NOTE(cmat): JS - WASM platform API.

function js_platform_set_shared_memory(frame_state_address) {
  wasm_context.shared_memory.frame_state = frame_state_address
}

// ------------------------------------------------------------
// #-- NOTE(cmat): JS - WASM webgpu API.

async function webgpu_init(canvas) {
  if (!navigator.gpu) {
    console.error("WebGPU unsupported");
    alert("WebGPU unsupported on this browser");
    return;
  }

  const webgpu_adapter = await navigator.gpu.requestAdapter();
  if (!webgpu_adapter) {
    console.error("Failed to get WebGPU adapter");
    alert("WebGPU unuspported on this browser (failed to get adapter)");
    return;
  }

  const webgpu_device  = await webgpu_adapter.requestDevice();

  const webgpu_context = wasm_context.canvas.getContext("webgpu");
  const webgpu_format  = navigator.gpu.getPreferredCanvasFormat();

  webgpu_context.configure({
    device: webgpu_device,
    format: webgpu_format,
    alphaMode: 'premultiplied'
  });

  webgpu_handle_map = {
    next_user_handle: 1,
    map:              new Map(),

    store(webgpu_handle) {
      const user_handle = this.next_user_handle++;
      this.map.set(user_handle, webgpu_handle);
      return user_handle;
    },

    get(user_handle)     { return this.map.get(user_handle); },
    remove(user_handle)  { this.map.delete(user_handle);     }
  };

  return {
    device:             webgpu_device,
    context:            webgpu_context,
    handle_map:         webgpu_handle_map,
    backbuffer_format:  webgpu_format,
  };
}

function js_webgpu_buffer_allocate(bytes, mode) {
  const buffer = wasm_context.webgpu.device.createBuffer({
    size:   bytes,
    usage:  GPUBufferUsage.VERTEX   |
            GPUBufferUsage.INDEX    | 
            GPUBufferUsage.STORAGE  |
            GPUBufferUsage.UNIFORM  |
            GPUBufferUsage.COPY_DST,
  });

  return wasm_context.webgpu.handle_map.store(buffer)
}

function js_webgpu_buffer_download(buffer_handle, offset, bytes, data_ptr) {
  const buffer = wasm_context.webgpu.handle_map.get(buffer_handle);
  const data   = new Uint8Array(wasm_context.memory.buffer, data_ptr, bytes);
  wasm_context.webgpu.device.queue.writeBuffer(buffer, offset, data, 0, bytes);
}

function js_webgpu_buffer_destroy(buffer_handle) {
  buffer = wasm_context.webgpu.handle_map.get(buffer_handle);
  wasm_context.webgpu.handle_map.remove(buffer_handle);
  buffer.destroy();
}

function js_webgpu_shader_create(shader_code_c_string_len, shader_code_c_string_str) {
  const shader_code = js_string_from_c_string(shader_code_c_string_len, shader_code_c_string_str);
  const shader = wasm_context.webgpu.device.createShaderModule({ code: shader_code });
  return wasm_context.webgpu.handle_map.store(shader);
}

function js_webgpu_shader_destroy(shader_handle) {
  shader = wasm_context.webgpu.handle_map.get(shader_handle);
  wasm_context.webgpu.handle_map.remove(shader_handle);
  shader.destroy();
}

const WebGPU_Texture_Format_Lookup_Name = [
  'rgba8unorm',
  'rgba8snorm',
  'r8unorm',
  'r8snorm',
];

const WebGPU_Texture_Format_Lookup_Bytes = [
  4,
  4,
  1,
  1
];

function js_webgpu_texture_allocate(format, width, height) {
  const texture = wasm_context.webgpu.device.createTexture({
    size:     [ width, height ],
    format:   WebGPU_Texture_Format_Lookup_Name[format],
    usage:    GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_DST,
  });

  return wasm_context.webgpu.handle_map.store(texture);
}

function js_webgpu_texture_download(texture_handle, download_format, region_x0, region_y0, region_x1, region_y1, data_ptr) {
  const texture = wasm_context.webgpu.handle_map.get(texture_handle);

  const region_width  = (region_x1 - region_x0);
  const region_height = (region_y1 - region_y0);
  const pixel_bytes   = WebGPU_Texture_Format_Lookup_Bytes[download_format];
  const region_bytes  = region_width * region_height * pixel_bytes;
  const data          = new Uint8Array(wasm_context.memory.buffer, data_ptr, region_bytes);

  wasm_context.webgpu.device.queue.writeTexture(
    { texture: texture, mipLevel: 0, origin: { x: region_x0, y: region_y0, z: 0, }, },
    data,
    { bytesPerRow: region_width * pixel_bytes, rowsPerImage: region_height, },
    { width: region_width, height: region_height, depthOrArrayLayers: 1, }
  );
}

function js_webgpu_texture_destroy(texture_handle) {
  const texture = wasm_context.webgpu.handle_map.get(texture_handle);
  wasm_context.webgpu.handle_map.remove(texture_handle);
  texture.destroy();
}

WebGPU_Sampler_Filter_Lookup_Mode = [
  'linear',
  'nearest'
]

function js_webgpu_sampler_create(mag_filter_mode, min_filter_mode) {
  const sampler = wasm_context.webgpu.device.createSampler({
    magFilter: WebGPU_Sampler_Filter_Lookup_Mode[mag_filter_mode],
    minFilter: WebGPU_Sampler_Filter_Lookup_Mode[min_filter_mode]
  });

  return wasm_context.webgpu.handle_map.store(sampler);
}

function js_webgpu_sampler_destroy(sampler_handle) {
  const sampler = wasm_context.webgpu.handle_map.get(sampler_handle);
  wasm_context.webgpu.handle_map.remove(sampler_handle);
  sampler.destroy();
}

const WebGPU_Vertex_Attribute_Format_Lookup_Name = [
  'float32',
  'float32x2',
  'float32x3',
  'float32x4',

  'uint16',
  'uint16x2',
  'uint16x3',
  'uint16x4',

  'uint32',
  'uint32x2',
  'uint32x3',
  'uint32x4',

  'uint32',
]

function js_webgpu_pipeline_create(shader_handle, vertex_format_ptr) {
  const pipeline_layout = wasm_context.webgpu.device.createPipelineLayout({
    bindGroupLayouts: [
      wasm_context.webgpu.device.createBindGroupLayout({
        entries: [
          {
            binding: 0,
            visibility: GPUShaderStage.FRAGMENT,
            texture: { sampleType: 'float', viewDimension: '2d' },
          },

          {
            binding: 1,
            visibility: GPUShaderStage.FRAGMENT,
            sampler: { type: 'filtering' },
          },

          {
            binding: 2,
            visibility: GPUShaderStage.VERTEX,
            buffer: { type: 'uniform' },
          }
        ]
      })
    ]
  });

  const vertex_format_view = new DataView(wasm_context.memory.buffer, vertex_format_ptr, 2 * 2 + 2 * 2 * 8);

  let offset = 0;
  const stride      = vertex_format_view.getUint16(offset, true); offset += 2;
  const entry_count = vertex_format_view.getUint16(offset, true); offset += 2;

  attribute_list = [ ]
  for (let it = 0; it < entry_count; it++) {
    const attribute_offset = vertex_format_view.getUint16(offset, true); offset += 2;
    const attribute_format = vertex_format_view.getUint16(offset, true); offset += 2;

    attribute_list.push({ shaderLocation: it, offset: attribute_offset, format: WebGPU_Vertex_Attribute_Format_Lookup_Name[attribute_format] });
  }

  const render_pipeline = wasm_context.webgpu.device.createRenderPipeline({
    layout: pipeline_layout,
    
    vertex: {
      module: wasm_context.webgpu.handle_map.get(shader_handle),
      entryPoint: 'vs_main',
      buffers: [
        {
          arrayStride: stride,
/*
          attributes: [
            { shaderLocation: 0, offset: 0,   format: 'float32x2' }, // X
            { shaderLocation: 1, offset: 16,  format: 'float32x2' }, // U
            { shaderLocation: 2, offset: 32,  format: 'uint32'    }, // C
          ]
*/
          attributes: attribute_list,
        }
      ]
    },

    fragment: {
      module: wasm_context.webgpu.handle_map.get(shader_handle),
      entryPoint: 'fs_main',
      targets: [
        {
          format: wasm_context.webgpu.backbuffer_format,
          blend: {
            color: { srcFactor: 'one', dstFactor: 'one-minus-src-alpha', operation: 'add' },
            alpha: { srcFactor: 'one', dstFactor: 'one-minus-src-alpha', operation: 'add' },
          }
        },
      ]
    },

    primitive: { topology: 'triangle-list' }
  });

  return wasm_context.webgpu.handle_map.store(render_pipeline);
}

function js_webgpu_pipeline_destroy(pipeline_handle) {
  const pipeline = wasm_context.webgpu.handle_map.get(pipeline_handle);
  wasm_context.webgpu.handle_map.remove(pipeline_handle);
}

function js_webgpu_frame_flush(draw_command_ptr) {
  const draw_command_view = new DataView(wasm_context.memory.buffer, draw_command_ptr, 17 * 4);
  
  let offset = 0;
  const constant_buffer_handle = draw_command_view.getUint32 (offset, true); offset += 4;
  const vertex_buffer_handle   = draw_command_view.getUint32 (offset, true); offset += 4;
  const index_buffer_handle    = draw_command_view.getUint32 (offset, true); offset += 4;
  const pipeline_handle        = draw_command_view.getUint32 (offset, true); offset += 4;
  const texture_handle         = draw_command_view.getUint32 (offset, true); offset += 4;
  const sampler_handle         = draw_command_view.getUint32 (offset, true); offset += 4;

  const draw_index_count       = draw_command_view.getUint32 (offset, true); offset += 4;
  const draw_index_offset      = draw_command_view.getUint32 (offset, true); offset += 4;

  const depth_test             = draw_command_view.getUint32 (offset, true); offset += 4;
  
  const draw_region_x0         = draw_command_view.getUint32 (offset, true); offset += 4;
  const draw_region_y0         = draw_command_view.getUint32 (offset, true); offset += 4;
  const draw_region_x1         = draw_command_view.getUint32 (offset, true); offset += 4;
  const draw_region_y1         = draw_command_view.getUint32 (offset, true); offset += 4;
 
  const clip_region_x0         = draw_command_view.getUint32 (offset, true); offset += 4;
  const clip_region_y0         = draw_command_view.getUint32 (offset, true); offset += 4;
  const clip_region_x1         = draw_command_view.getUint32 (offset, true); offset += 4;
  const clip_region_y1         = draw_command_view.getUint32 (offset, true); offset += 4;

  // NOTE(cmat): Retrieve handles.
  const constant_buffer = wasm_context.webgpu.handle_map.get(constant_buffer_handle);
  const vertex_buffer   = wasm_context.webgpu.handle_map.get(vertex_buffer_handle);
  const index_buffer    = wasm_context.webgpu.handle_map.get(index_buffer_handle);
  const pipeline        = wasm_context.webgpu.handle_map.get(pipeline_handle);
  const texture         = wasm_context.webgpu.handle_map.get(texture_handle);
  const sampler         = wasm_context.webgpu.handle_map.get(sampler_handle);

  const draw_region_width  = draw_region_x1 - draw_region_x0;
  const draw_region_height = draw_region_y1 - draw_region_y0;

  const clip_region_width  = clip_region_x1 - clip_region_x0;
  const clip_region_height = clip_region_y1 - clip_region_y0;

  const bind_group = wasm_context.webgpu.device.createBindGroup({
    layout: pipeline.getBindGroupLayout(0),
    entries: [ 
      { binding: 0, resource: texture.createView(), },
      { binding: 1, resource: sampler, },
      { binding: 2, resource: { buffer: constant_buffer } },
    ]
  });

  // const pass_encoder = command_encoder.beginRenderPass(render_pass_descriptor);

  // NOTE(cmat): Viewport.
  wasm_context.webgpu_pass_encoder.setViewport(draw_region_x0, draw_region_y0, draw_region_width, draw_region_height, 0.0, 1.0);
  wasm_context.webgpu_pass_encoder.setScissorRect(clip_region_x0, clip_region_y0, clip_region_width, clip_region_height);

  wasm_context.webgpu_pass_encoder.setPipeline(pipeline);
  wasm_context.webgpu_pass_encoder.setBindGroup(0, bind_group);
  wasm_context.webgpu_pass_encoder.setVertexBuffer(0, vertex_buffer);
  wasm_context.webgpu_pass_encoder.setIndexBuffer(index_buffer, "uint32");
  wasm_context.webgpu_pass_encoder.drawIndexed(draw_index_count, 1, draw_index_offset, 0, 0);

  // pass_encoder.end();

  // wasm_context.webgpu.device.queue.submit([command_encoder.finish()]);
}

function wasm_pack_frame_state(frame_state) {
  const buffer_view = new DataView(wasm_context.memory.buffer, wasm_context.shared_memory.frame_state, 8 * 4);
  
  let offset = 0;
  buffer_view.setUint32   (offset, frame_state.display.resolution.width,  true); offset += 4;
  buffer_view.setUint32   (offset, frame_state.display.resolution.height, true); offset += 4;
  buffer_view.setFloat32  (offset, frame_state.display.frame_delta,       true); offset += 4;

  buffer_view.setUint32   (offset, frame_state.input.mouse.position.x,    true); offset += 4;
  buffer_view.setUint32   (offset, frame_state.input.mouse.position.y,    true); offset += 4;
  buffer_view.setUint32   (offset, frame_state.input.mouse.button.left,   true); offset += 4;
  buffer_view.setUint32   (offset, frame_state.input.mouse.button.right,  true); offset += 4;
  buffer_view.setUint32   (offset, frame_state.input.mouse.button.middle, true); offset += 4;

  return buffer_view;
}

function canvas_next_frame() {

  frametime_now                                 = performance.now();
  wasm_context.frame_state.display.frame_delta  = 0.001 * (frametime_now - wasm_context.frame_time_last);
  wasm_context.frame_time_last                  = frametime_now;

  wasm_pack_frame_state(wasm_context.frame_state)
  const command_encoder = wasm_context.webgpu.device.createCommandEncoder();

  const backbuffer_texture_view = wasm_context.webgpu.context.getCurrentTexture().createView();
  const render_pass_descriptor = {
    colorAttachments: [{
      view: backbuffer_texture_view,
      clearValue: { r:0, g:0, b:0, a:1 },
      loadOp: 'clear',
      storeOp: 'store'
    }]
  };

  wasm_context.webgpu_pass_encoder = command_encoder.beginRenderPass(render_pass_descriptor);
  wasm_context.export_table.wasm_next_frame();
  wasm_context.webgpu_pass_encoder.end();
  wasm_context.webgpu.device.queue.submit([command_encoder.finish()]);

  requestAnimationFrame(canvas_next_frame);
}

function window_resolution_pixels() {
  const device_pixel_ratio = window.devicePixelRatio || 1;
  const width              = Math.round(window.innerWidth * device_pixel_ratio);
  const height             = Math.round(window.innerHeight * device_pixel_ratio);
  return [width, height];
}

function wasm_module_load(wasm_bytecode) {
  const memory = new WebAssembly.Memory({
    initial: 256,   // NOTE(cmat): 16 Megabytes
    maximum: 65536, // NOTE(cmat): 4 Gigabytes.
    shared:  false,
  });

  const import_table = {
    env: {
      memory: memory,

      // NOTE(cmat): Core API.
      js_core_stream_write:           js_core_stream_write,
      js_core_unix_time:              js_core_unix_time,
      js_core_panic:                  js_core_panic,

      // NOTE(cmat): Platform API.
      js_platform_set_shared_memory:  js_platform_set_shared_memory,

      // NOTE(cmat): WebGPU API.
      js_webgpu_buffer_allocate:      js_webgpu_buffer_allocate,
      js_webgpu_buffer_download:      js_webgpu_buffer_download,
      js_webgpu_buffer_destroy:       js_webgpu_buffer_destroy,

      js_webgpu_texture_allocate:     js_webgpu_texture_allocate,
      js_webgpu_texture_download:     js_webgpu_texture_download,
      js_webgpu_texture_destroy:      js_webgpu_texture_destroy,

      js_webgpu_sampler_create:       js_webgpu_sampler_create,
      js_webgpu_sampler_destroy:      js_webgpu_sampler_destroy,

      js_webgpu_shader_create:        js_webgpu_shader_create,
      js_webgpu_shader_destroy:       js_webgpu_shader_destroy,

      js_webgpu_pipeline_create:      js_webgpu_pipeline_create,
      js_webgpu_pipeline_destroy:     js_webgpu_pipeline_destroy,

      js_webgpu_frame_flush:          js_webgpu_frame_flush,
    }
  };

  WebAssembly.instantiate(wasm_bytecode, import_table).then(wasm => {
    wasm_context.memory       = wasm.instance.exports.memory;
    wasm_context.export_table = wasm.instance.exports;
    wasm_context.canvas       = document.getElementById("alice_canvas");
    
    webgpu_init(wasm_context.canvas).then(webgpu => {
      wasm_context.webgpu = webgpu;
      
      const resolution            = window_resolution_pixels();
      wasm_context.canvas.width   = resolution[0];
      wasm_context.canvas.height  = resolution[1];

      wasm_context.frame_state.display.resolution.width  = wasm_context.canvas.width;
      wasm_context.frame_state.display.resolution.height = wasm_context.canvas.height;

      // NOTE(cmat): Dynamically modify canvas resolution.
      window.addEventListener('resize', () => {
        const resolution = window_resolution_pixels();
        wasm_context.canvas.width  = resolution[0];
        wasm_context.canvas.height = resolution[1];

        wasm_context.frame_state.display.resolution.width  = wasm_context.canvas.width;
        wasm_context.frame_state.display.resolution.height = wasm_context.canvas.height;
      });

      // NOTE(cmat): Handle mouse events.
      window.addEventListener('mousemove', e => {
        const client_rect = wasm_context.canvas.getBoundingClientRect();
        const scale_x     = wasm_context.canvas.width / client_rect.width;
        const scale_y     = wasm_context.canvas.height / client_rect.height;

        wasm_context.frame_state.input.mouse.position.x = (e.clientX - client_rect.left) * scale_x;
        wasm_context.frame_state.input.mouse.position.y = wasm_context.canvas.height - (e.clientY - client_rect.top)  * scale_y;
      });

      window.addEventListener('mousedown', e => {
        if (e.button == 0) { wasm_context.frame_state.input.mouse.button.left   = 1 }
        if (e.button == 1) { wasm_context.frame_state.input.mouse.button.middle = 1 }
        if (e.button == 2) { wasm_context.frame_state.input.mouse.button.right  = 1 }
      });

      window.addEventListener('mouseup', e => {
        if (e.button == 0) { wasm_context.frame_state.input.mouse.button.left   = 0 }
        if (e.button == 1) { wasm_context.frame_state.input.mouse.button.middle = 0 }
        if (e.button == 2) { wasm_context.frame_state.input.mouse.button.right  = 0 }
      });

      // NOTE(cmat): Disable context menu on canvas.
      wasm_context.canvas.addEventListener('contextmenu', e => e.preventDefault());

      // NOTE(cmat): Call into entry point
      const cpu_logical_cores = navigator.hardwareConcurrency;
      wasm_context.export_table.wasm_entry_point(cpu_logical_cores);

      // NOTE(cmat): Start animation frame requests
      wasm_context.frame_time_last = performance.now();
      canvas_next_frame();
    });
  });
}

// NOTE(cmat): Load WASM module.
fetch("alice_canvas.wasm")
  .then(response => response.arrayBuffer())
  .then(bytes    => wasm_module_load(bytes));
