const wasm_context = {
  canvas:         null,
  memory:         null,
  export_table:   null,
  webgpu_context: null,
  webgpu_device:  null,
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
    alphaMode: 'opaque'
  });

  return {
    device:   webgpu_device,
    context:  webgpu_context
  };
}

function webgpu_frame_flush() {
  const command_encoder = wasm_context.webgpu_device.createCommandEncoder();
  const render_pass_descriptor = {
    colorAttachments: [{
      view: wasm_context.webgpu_context.getCurrentTexture().createView(),
      clearValue: { r:0, g:0, b:1, a:1 },
      loadOp: 'clear',
      storeOp: 'store'
    }]
  };

  const pass_encoder = command_encoder.beginRenderPass(render_pass_descriptor);
  pass_encoder.end();

  wasm_context.webgpu_device.queue.submit([command_encoder.finish()]);
}

function canvas_next_frame() {
  requestAnimationFrame(canvas_next_frame);
  wasm_context.export_table.wasm_next_frame();
  webgpu_frame_flush();
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

  WebAssembly.instantiate(wasm_bytecode, import_table)
    .then(wasm => {
      wasm_context.memory       = wasm.instance.exports.memory;
      wasm_context.export_table = wasm.instance.exports;
      wasm_context.canvas       = document.getElementById("alice_canvas");
      
      webgpu_init(wasm_context.canvas).then(webgpu => {
        wasm_context.webgpu_device  = webgpu.device;
        wasm_context.webgpu_context = webgpu.context;

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

        // NOTE(cmat): Call into entry point.
        wasm_context.export_table.wasm_entry_point();

        // NOTE(cmat): Start animation frame requests
        canvas_next_frame();
      });
    });
}

// NOTE(cmat): Load WASM module.
fetch("alice_canvas.wasm")
  .then(response => response.arrayBuffer())
  .then(bytes    => wasm_module_load(bytes));
