const wasm_context = {
  canvas:         null,
  memory:         null,
  export_table:   null,
  webgpu:         null,
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
// #-- NOTE(cmat): WebGPU backend implementation.

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

function webgpu_buffer_allocate(bytes) {
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

function webgpu_buffer_download(buffer_handle, offset, bytes, data) {
  buffer = wasm_context.webgpu.handle_map.get(buffer_handle);
  wasm_context.webgpu.device.queue.writeBuffer(buffer, offset, data, 0, bytes);
}

function webgpu_buffer_destroy(buffer_handle) {
  buffer = wasm_context.webgpu.handle_map.get(buffer_handle);
  wasm_context.webgpu.handle_map.remove(buffer_handle);

  buffer.destroy();
}

function webgpu_shader_create(shader_code) {
  shader = wasm_context.webgpu.device.createShaderModule({ code: shader_code });
  return wasm_context.webgpu.handle_map.store(shader);
}

vertex_buffer          = null;
index_buffer           = null;
shader_flat_2D         = null;
pipeline_flat2D        = null;
bindgroup_flat2D       = null;
texture_white          = null;
sampler_linear         = null;
buffer_NDC_from_screen = null;

const vertices        = new ArrayBuffer(20 * 4);
const vertices_view   = new DataView(vertices);

function vertex_push(idx, x, y, u, v, packed_color) {
  let off = idx * 20;
  vertices_view.setFloat32 (off + 0,  x,            true);
  vertices_view.setFloat32 (off + 4,  y,            true);
  vertices_view.setFloat32 (off + 8,  u,            true);
  vertices_view.setFloat32 (off + 12, v,            true);
  vertices_view.setUint32  (off + 16, packed_color, true);
}

function webgpu_setup() {

  identity_data = new Float32Array([
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1,
  ]);

  identity_buffer = new Uint8Array(identity_data.buffer, identity_data.byteOffset, identity_data.byteLength);

  buffer_NDC_from_screen = webgpu_buffer_allocate(256);
  webgpu_buffer_download(buffer_NDC_from_screen, 0, identity_buffer.byteLength, identity_buffer);

  texture_white = wasm_context.webgpu.device.createTexture({
    size: [ 2, 2 ],
    format: 'rgba8unorm',
    usage: GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_DST,
  });

  const white_pixels = new Uint8Array(2 * 2 * 4);
  white_pixels[0]  = 255;
  white_pixels[1]  = 0;
  white_pixels[2]  = 0;
  white_pixels[3]  = 255;

  white_pixels[4]  = 0;
  white_pixels[5]  = 255;
  white_pixels[6]  = 0;
  white_pixels[7]  = 255;

  white_pixels[8]  = 0;
  white_pixels[9]  = 0;
  white_pixels[10] = 255;
  white_pixels[11] = 255;

  white_pixels[12] = 255;
  white_pixels[13] = 255;
  white_pixels[14] = 255;
  white_pixels[15] = 255;

  wasm_context.webgpu.device.queue.writeTexture(
    { texture: texture_white, },
    white_pixels,
    { bytesPerRow: 2 * 4, rowsPerImage: 2, },
    { width: 2, height: 2, depthOrArrayLayers: 1, }
  );

  sampler_linear = wasm_context.webgpu.device.createSampler({
    magFilter: 'linear',
    minFilter: 'linear',
  });

  vertex_push(0, -0.5, -0.5, 0.0, 0.0, 0xFFFF0000);
  vertex_push(1,  0.5, -0.5, 1.0, 0.0, 0xFF00FF00);
  vertex_push(2,  0.5,  0.5, 0.5, 1.0, 0xFF0000FF);
  vertex_push(3, -0.5,  0.5, 0.5, 1.0, 0xFF0000FF);

  vertex_buffer = webgpu_buffer_allocate(vertices.byteLength);
  webgpu_buffer_download(vertex_buffer, 0, vertices.byteLength, vertices);

  index_data = new Int32Array([ 0, 1, 2, 0, 2, 3 ]);
  index_view = new Uint8Array(index_data.buffer, index_data.byteOffset, index_data.byteLength);

  index_buffer = webgpu_buffer_allocate(index_view.byteLength);
  webgpu_buffer_download(index_buffer, 0, index_view.byteLength, index_view);

  shader_code = `

  @group(0) @binding(0)
  var Texture : texture_2d<f32>;
  
  @group(0) @binding(1)
  var Sampler : sampler;

  @group(0) @binding(2)
  var<uniform> NDC_From_Screen : mat4x4<f32>;

  fn vec4_unpack_u32(packed: u32) -> vec4<f32> {
    let r = f32((packed >> 0)  & 0xFFu) / 255.0;
    let g = f32((packed >> 8)  & 0xFFu) / 255.0;
    let b = f32((packed >> 16) & 0xFFu) / 255.0;
    let a = f32((packed >> 24) & 0xFFu) / 255.0;
    
    return vec4<f32>(r, g, b, a);
  }

  struct VS_Out {
    @builtin(position)  X : vec4<f32>,
    @location(0)        C : vec4<f32>,
    @location(1)        U : vec2<f32>,
  };

  @vertex
  fn vs_main(@location(0) X : vec2<f32>,
             @location(1) U : vec2<f32>,
             @location(2) C : u32) -> VS_Out {

    var out : VS_Out;

    out.X = NDC_From_Screen * vec4<f32>(X, 0.0, 1.0);
    out.C = vec4_unpack_u32(C);
    out.U = U;

    return out;
  }

  @fragment
  fn fs_main(@location(0) C : vec4<f32>,
             @location(1) U : vec2<f32> ) -> @location(0) vec4<f32> {

    let color_texture = textureSample(Texture, Sampler, U);
    let color =  color_texture * C;
    return color;
  }`;

  shader_flat_2D = webgpu_shader_create(shader_code);

  pipeline_layout = wasm_context.webgpu.device.createPipelineLayout({
    bindGroupLayouts: [
      wasm_context.webgpu.device.createBindGroupLayout({
        entries: [
          {
            binding: 0,
            visibility: GPUShaderStage.FRAGMENT,
            texture: {
              sampleType: 'float',
              viewDimension: '2d'
            },

          },

          {
            binding: 1,
            visibility: GPUShaderStage.FRAGMENT,
            sampler: {
              type: 'filtering'
            },
          },

          {
            binding: 2,
            visibility: GPUShaderStage.VERTEX,
            buffer: {
              type: 'uniform'
            },
          }

        ]
      })
    ]
  });

  pipeline = wasm_context.webgpu.device.createRenderPipeline({
    layout: pipeline_layout,
    
    vertex: {
      module: wasm_context.webgpu.handle_map.get(shader_flat_2D),
      entryPoint: 'vs_main',
      buffers: [
        {
          arrayStride: 20,
          attributes: [
            { shaderLocation: 0, offset: 0,   format: 'float32x2' }, // X
            { shaderLocation: 1, offset: 8,   format: 'float32x2' }, // U
            { shaderLocation: 2, offset: 16,  format: 'uint32'    }, // C
          ]
        }
      ]
    },

    fragment: {
      module: wasm_context.webgpu.handle_map.get(shader_flat_2D),
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

  bindgroup_flat_2D = wasm_context.webgpu.device.createBindGroup({
    layout: pipeline.getBindGroupLayout(0),
    entries: [ 
      { binding: 0, resource: texture_white.createView(), },
      { binding: 1, resource: sampler_linear, },
      { binding: 2, resource: { buffer: wasm_context.webgpu.handle_map.get(buffer_NDC_from_screen) }, },
    ]
  });


}

function webgpu_frame_flush() {
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

  const pass_encoder = command_encoder.beginRenderPass(render_pass_descriptor);

  // NOTE(cmat): Viewport.
  pass_encoder.setViewport(0, 0, wasm_context.canvas.width, wasm_context.canvas.height, 0.0, 1.0);
  // pass_encoder.setScissorRect(0, 0, wasm_context.canvas.width / 2, wasm_context.canvas.height);

  pass_encoder.setPipeline(pipeline);
  pass_encoder.setBindGroup(0, bindgroup_flat_2D);
  pass_encoder.setVertexBuffer(0, wasm_context.webgpu.handle_map.get(vertex_buffer));
  pass_encoder.setIndexBuffer(wasm_context.webgpu.handle_map.get(index_buffer), "uint32");
  pass_encoder.drawIndexed(6, 1, 0, 0, 0);

  pass_encoder.end();

  wasm_context.webgpu.device.queue.submit([command_encoder.finish()]);
}

function canvas_next_frame() {
  wasm_context.export_table.wasm_next_frame();
  webgpu_frame_flush();

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
    shared:  false
  });

  const import_table = {
    env: {
      memory: memory,

      js_core_stream_write: js_core_stream_write,
      js_core_unix_time:    js_core_unix_time,
      js_core_panic:        js_core_panic,
    }
  };

  WebAssembly.instantiate(wasm_bytecode, import_table).then(wasm => {
    wasm_context.memory       = wasm.instance.exports.memory;
    wasm_context.export_table = wasm.instance.exports;
    wasm_context.canvas       = document.getElementById("alice_canvas");
    
    webgpu_init(wasm_context.canvas).then(webgpu => {
      wasm_context.webgpu = webgpu;
      
      webgpu_setup();

      const resolution            = window_resolution_pixels();
      wasm_context.canvas.width   = resolution[0];
      wasm_context.canvas.height  = resolution[1];

      // NOTE(cmat): Dynamically modify canvas resolution.
      window.addEventListener('resize', () => {
        const resolution = window_resolution_pixels();
        wasm_context.canvas.width  = resolution[0];
        wasm_context.canvas.height = resolution[1];
      });

      // NOTE(cmat): Disable context menu on canvas.
      wasm_context.canvas.addEventListener('contextmenu', e => e.preventDefault());

      // NOTE(cmat): Call into entry point
      const cpu_logical_cores = navigator.hardwareConcurrency;
      wasm_context.export_table.wasm_entry_point(cpu_logical_cores);

      // NOTE(cmat): Start animation frame requests
      canvas_next_frame();
    });
  });
}

// NOTE(cmat): Load WASM module.
fetch("alice_canvas.wasm")
  .then(response => response.arrayBuffer())
  .then(bytes    => wasm_module_load(bytes));
