// (C) Copyright 2025 Matyas Constans
// Licensed under the MIT License (https://opensource.org/license/mit/)

R_Shader  R_Shader_Flat_2D        = { };
R_Shader  R_Shader_Flat_3D        = { };
R_Shader  R_Shader_MTSDF_2D       = { };
R_Texture R_Texture_White         = { };
R_Sampler R_Sampler_Linear_Clamp  = { };
R_Sampler R_Sampler_Nearest_Clamp = { };

#define Metal_Max_Buffers   1024
#define Metal_Max_Shaders   256
#define Metal_Max_Samplers  64
#define Metal_Max_Textures  1024
#define Metal_Max_Pipelines 512

typedef struct Metal_Buffer {
  id<MTLBuffer> buffer;
  U64 capacity;
  volatile B32 download_complete;
} Metal_Buffer;

typedef struct Metal_Shader {
  id<MTLLibrary> base_lib;
  id<MTLFunction> vertex_entry;
  id<MTLFunction> fragment_entry;
} Metal_Shader;

typedef struct Metal_Sampler {
  id<MTLSamplerState> sampler;
} Metal_Sampler;

typedef struct Metal_Texture {
  id<MTLTexture> texture;
  U32 width;
  U32 height;
  volatile B32 download_complete;
} Metal_Texture;

typedef struct Metal_Pipeline {
  id<MTLRenderPipelineState> pipeline;
} Metal_Pipeline;

var_global struct {
  B32 initialized;

  CAMetalLayer *layer;
  id<MTLDevice> device;
  id<MTLCommandQueue> command_queue;

  U32 buffer_last_id;
  U32 shader_last_id;
  U32 sampler_last_id;
  U32 texture_last_id;
  U32 pipeline_last_id;

  Metal_Buffer   buffers   [Metal_Max_Buffers];
  Metal_Shader   shaders   [Metal_Max_Shaders];
  Metal_Sampler  samplers  [Metal_Max_Samplers];
  Metal_Texture  textures  [Metal_Max_Textures];
  Metal_Pipeline pipelines [Metal_Max_Pipelines];

  id<MTLTexture>           depth_texture;
  id<MTLDepthStencilState> depth_stencil_state;
  id<MTLDepthStencilState> depth_stencil_none_state;
} Metal_State = { };

// NOTE(cmat): Buffers.
// #--
fn_internal R_Buffer r_buffer_allocate(U64 capacity, R_Buffer_Mode mode) {
  Metal_Buffer *metal_buffer = &Metal_State.buffers[Metal_State.buffer_last_id++];
  metal_buffer->buffer = [Metal_State.device newBufferWithLength:capacity options:MTLResourceStorageModeShared];
  Assert(metal_buffer->buffer, "error while creating metal buffer");
  
  R_Buffer buffer = { .id = Metal_State.buffer_last_id };
  return buffer;
}

fn_internal void r_buffer_download(R_Buffer *buffer, U64 offset, U64 bytes, void *data) {
  Assert(buffer->id, "invalid buffer");
  Metal_Buffer *metal_buffer = &Metal_State.buffers[buffer->id - 1];
  id<MTLBuffer> upload_buffer = [Metal_State.device newBufferWithBytes:data
                                                                length:bytes
                                                                options:MTLResourceStorageModeShared];

  id<MTLCommandBuffer> command_buffer = [Metal_State.command_queue commandBuffer];
  id<MTLBlitCommandEncoder> encoder = [command_buffer blitCommandEncoder];

  [encoder copyFromBuffer:upload_buffer
             sourceOffset:0
                 toBuffer:metal_buffer->buffer
        destinationOffset:offset
                     size:bytes];

  [encoder endEncoding];
  [command_buffer addCompletedHandler:^(id<MTLCommandBuffer> cb) { metal_buffer->download_complete = 1; }];
  [command_buffer commit];
}

// TODO(cmat): Implement this.
fn_internal void r_buffer_destroy(R_Buffer *buffer) {
}

// NOTE(cmat): Textures.
// #--
fn_internal R_Texture r_texture_allocate(R_Texture_Config *config) {
  Metal_Texture *texture = &Metal_State.textures[Metal_State.texture_last_id++];
  @autoreleasepool {

    var_local_persist MTLPixelFormat metal_format_lookup[] = {
      MTLPixelFormatRGBA8Unorm, // R_Texture_Format_RGBA_U08_Normalized,
      MTLPixelFormatRGBA8Snorm, // R_Texture_Format_RGBA_I08_Normalized,
      MTLPixelFormatR8Unorm,    // R_Texture_Format_R_U08_Normalized,
      MTLPixelFormatR8Snorm,    // R_Texture_Format_R_I08_Normalized,
    };
    
    MTLTextureDescriptor *texture_desc = [[[MTLTextureDescriptor alloc] init] autorelease];
    texture_desc.pixelFormat = metal_format_lookup[config->format];
    texture_desc.width = config->width;
    texture_desc.height = config->height;
    texture_desc.usage = MTLTextureUsageShaderRead;
    texture_desc.storageMode = MTLStorageModePrivate;
    texture_desc.textureType = MTLTextureType2D;

    texture->texture = [Metal_State.device newTextureWithDescriptor:texture_desc];
    texture->width = config->width;
    texture->height = config->height;
  }

  R_Texture r_texture = {
    .id    = Metal_State.texture_last_id,
  };

  return r_texture;
}


fn_internal void r_texture_download(R_Texture *texture, U08 *texture_data) {
  Metal_Texture *metal_texture = &Metal_State.textures[texture->id - 1];

  // TODO(cmat): Assuming 4 channels this is wrong.
  U64 texture_bytes = metal_texture->width * metal_texture->height * 4;

  id<MTLBuffer> upload_buffer = [Metal_State.device newBufferWithLength:texture_bytes options:MTLResourceStorageModeShared];
  memcpy(upload_buffer.contents, texture_data, texture_bytes);

  id<MTLCommandBuffer>      command_buffer = [Metal_State.command_queue commandBuffer];
  id<MTLBlitCommandEncoder> blit_encoder   = [command_buffer blitCommandEncoder];

  MTLSize copy_size = { metal_texture->width, metal_texture->height, 1 };
  [blit_encoder copyFromBuffer:upload_buffer
                 sourceOffset:0
            sourceBytesPerRow:metal_texture->width * 4
          sourceBytesPerImage:metal_texture->width * metal_texture->height * 4
                   sourceSize:copy_size
                    toTexture:metal_texture->texture
             destinationSlice:0
             destinationLevel:0
            destinationOrigin:(MTLOrigin) { 0, 0, 0 }];

  [blit_encoder endEncoding];
  [command_buffer addCompletedHandler:^(id<MTLCommandBuffer> cb) { }];
  [command_buffer commit];
}

// NOTE(cmat): Pipelines.
// #--
fn_internal R_Pipeline r_pipeline_create(R_Shader shader, R_Vertex_Format format) {
  Metal_Pipeline *metal_pipeline = &Metal_State.pipelines[Metal_State.pipeline_last_id++];
  
  @autoreleasepool {
    MTLVertexDescriptor *vertex_desc = [[[MTLVertexDescriptor alloc] init] autorelease];
    var_local_persist MTLVertexFormat metal_format_lookup[] = {
        MTLVertexFormatFloat,  // R_Vertex_Attribute_F32
        MTLVertexFormatFloat2, // R_Vertex_Attribute_V2_F32,
        MTLVertexFormatFloat3, // R_Vertex_Attribute_V3_F32,
        MTLVertexFormatFloat4, // R_Vertex_Attribute_V4_F32,

        MTLVertexFormatUShort,  // R_Vertex_Attribute_U16,
        MTLVertexFormatUShort2, // R_Vertex_Attribute_V2_U16,
        MTLVertexFormatUShort3, // R_Vertex_Attribute_V3_U16,
        MTLVertexFormatUShort4, // R_Vertex_Attribute_V4_U16,

        MTLVertexFormatUInt,  // R_Vertex_Attribute_U32,
        MTLVertexFormatUInt2, // R_Vertex_Attribute_V2_U32,
        MTLVertexFormatUInt3, // R_Vertex_Attribute_V3_U32,
        MTLVertexFormatUInt4, // R_Vertex_Attribute_V4_U32,
        
        MTLVertexFormatUChar4Normalized, // R_Vertex_Attribute_V4_U08_Normalized
    };
    
    For_U32(it, format.entry_count) {
        vertex_desc.attributes[it].format = metal_format_lookup[format.entry_array[it].format];
        vertex_desc.attributes[it].offset = format.entry_array[it].offset;
        vertex_desc.attributes[it].bufferIndex = 1;
    }

    vertex_desc.layouts[1].stride = format.stride;
    vertex_desc.layouts[1].stepFunction = MTLVertexStepFunctionPerVertex;

    // NOTE(cmat): Configure pipeline.
    Assert(shader.id, "invalid shader");
    Metal_Shader *metal_shader = &Metal_State.shaders[shader.id - 1];
    
    MTLRenderPipelineDescriptor *pipeline_desc = [[MTLRenderPipelineDescriptor alloc] init];
    pipeline_desc.vertexFunction = metal_shader->vertex_entry;
    pipeline_desc.fragmentFunction = metal_shader->fragment_entry;
    pipeline_desc.vertexDescriptor = vertex_desc;
    pipeline_desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
    
    pipeline_desc.colorAttachments[0].blendingEnabled = YES;
    pipeline_desc.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
    pipeline_desc.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;

    pipeline_desc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    pipeline_desc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
            
    pipeline_desc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
    pipeline_desc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorZero;

    pipeline_desc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
    
    // NOTE(cmat): Create pipeline.
    NSError *pipeline_error = 0;
    metal_pipeline->pipeline = [Metal_State.device newRenderPipelineStateWithDescriptor:pipeline_desc error:&pipeline_error];

    if (pipeline_error) {
      NSLog(@"Failed to create pipeline state: %@", pipeline_error);
    }

    
    if (!metal_pipeline->pipeline) {
      Assert(!metal_pipeline->pipeline, "invalid metal pipeline");
    }
  }

  return (R_Pipeline) { .id = Metal_State.pipeline_last_id, };
}

// NOTE(cmat): Default resources initialization.
// #--

fn_internal void metal_create_default_shaders(void) {
  NSError *lib_error = 0;
  dispatch_data_t lib_data = dispatch_data_create(metal_baked_shaders_data, metal_baked_shaders_bytes, 0, 0);
  id<MTLLibrary> lib = [Metal_State.device newLibraryWithData:lib_data error:&lib_error];
  Assert(lib, "failed to initialize default shaders");

  Metal_Shader *flat_2D_shader = &Metal_State.shaders[Metal_State.shader_last_id++]; {
    flat_2D_shader->base_lib = lib;
    flat_2D_shader->vertex_entry = [lib newFunctionWithName:@"flat_2D_vertex"];
    flat_2D_shader->fragment_entry = [lib newFunctionWithName:@"flat_2D_pixel"];

    R_Shader_Flat_2D = (R_Shader) { .id = Metal_State.shader_last_id, };
  }

  Metal_Shader *flat_3D_shader = &Metal_State.shaders[Metal_State.shader_last_id++]; {
    flat_3D_shader->base_lib = lib;
    flat_3D_shader->vertex_entry = [lib newFunctionWithName:@"flat_3D_vertex"];
    flat_3D_shader->fragment_entry = [lib newFunctionWithName:@"flat_3D_pixel"];

    R_Shader_Flat_3D = (R_Shader) { .id = Metal_State.shader_last_id, };
  }
 
  Metal_Shader *mtsdf_2D_shader = &Metal_State.shaders[Metal_State.shader_last_id++]; {
    mtsdf_2D_shader->base_lib = lib;
    mtsdf_2D_shader->vertex_entry = [lib newFunctionWithName:@"mtsdf_2D_vertex"];
    mtsdf_2D_shader->fragment_entry = [lib newFunctionWithName:@"mtsdf_2D_pixel"];

    R_Shader_MTSDF_2D = (R_Shader) { .id = Metal_State.shader_last_id, };
  }
}

fn_internal void metal_create_default_samplers(void) {
  Metal_Sampler *linear_clamp = &Metal_State.samplers[Metal_State.sampler_last_id++]; {
    MTLSamplerDescriptor *sampler_desc = [[[MTLSamplerDescriptor alloc] init] autorelease];
    sampler_desc.minFilter = MTLSamplerMinMagFilterLinear;
    sampler_desc.magFilter = MTLSamplerMinMagFilterLinear;
    sampler_desc.mipFilter = MTLSamplerMipFilterLinear;
    sampler_desc.sAddressMode = MTLSamplerAddressModeClampToEdge;
    sampler_desc.tAddressMode = MTLSamplerAddressModeClampToEdge;
    sampler_desc.lodMinClamp = 0;
    sampler_desc.lodMaxClamp = 10.0;
    sampler_desc.maxAnisotropy = 4;
    
    linear_clamp->sampler = [Metal_State.device newSamplerStateWithDescriptor:sampler_desc];
    R_Sampler_Linear_Clamp = (R_Sampler) { .id = Metal_State.sampler_last_id, };
  }

  Metal_Sampler *nearest_clamp = &Metal_State.samplers[Metal_State.sampler_last_id++]; {
    MTLSamplerDescriptor *sampler_desc = [[[MTLSamplerDescriptor alloc] init] autorelease];
    sampler_desc.minFilter = MTLSamplerMinMagFilterNearest;
    sampler_desc.magFilter = MTLSamplerMinMagFilterNearest;
    sampler_desc.mipFilter = MTLSamplerMipFilterNearest;
    sampler_desc.sAddressMode = MTLSamplerAddressModeClampToEdge;
    sampler_desc.tAddressMode = MTLSamplerAddressModeClampToEdge;
    sampler_desc.lodMinClamp = 0;
    sampler_desc.lodMaxClamp = 10.0;
    sampler_desc.maxAnisotropy = 4;
    
    nearest_clamp->sampler = [Metal_State.device newSamplerStateWithDescriptor:sampler_desc];
    R_Sampler_Nearest_Clamp = (R_Sampler) { .id = Metal_State.sampler_last_id, };
  }
}

fn_internal void metal_create_default_textures(void) {
  R_Texture_Config white_texture_conf = {
    .format = R_Texture_Format_RGBA_U08_Normalized,
    .width  = 2,
    .height = 2,
  };

  U32 white_texture_data[] = {
    0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF,
  };
  
  R_Texture_White = r_texture_allocate(&white_texture_conf);
  r_texture_download(&R_Texture_White, (U08 *)white_texture_data);
}

// NOTE(cmat): Begin frame, End frame.
// #--
fn_internal void r_init(PL_Render_Context *context) {
  If_Unlikely(!Metal_State.initialized) {
    @autoreleasepool {
      Metal_State.initialized = 1;
      Metal_State.device = (id<MTLDevice>)context->metal_device;
      Metal_State.layer = (CAMetalLayer *)context->metal_layer;
      Metal_State.command_queue = [Metal_State.device newCommandQueue];
     
      metal_create_default_shaders();
      metal_create_default_samplers();
      metal_create_default_textures();

      MTLDepthStencilDescriptor *depth_stencil_desc = [[MTLDepthStencilDescriptor alloc] init];
      depth_stencil_desc.depthCompareFunction = MTLCompareFunctionLess;
      depth_stencil_desc.depthWriteEnabled = YES;
      Metal_State.depth_stencil_state = [Metal_State.device newDepthStencilStateWithDescriptor:depth_stencil_desc];
 

      MTLDepthStencilDescriptor *depth_stencil_none_desc = [[MTLDepthStencilDescriptor alloc] init];
      depth_stencil_none_desc.depthCompareFunction = MTLCompareFunctionAlways;
      depth_stencil_none_desc.depthWriteEnabled    = NO;
      Metal_State.depth_stencil_none_state         = [Metal_State.device newDepthStencilStateWithDescriptor:depth_stencil_none_desc];
    }
  }
}

fn_internal void r_frame_flush(void) {
  V2F display_size = pl_display()->resolution;
  
  @autoreleasepool {
    id<CAMetalDrawable> drawable = [Metal_State.layer nextDrawable];
      if (drawable) {

        // NOTE(cmat): Default depth texture.
        if (!Metal_State.depth_texture ||
            (Metal_State.depth_texture.width  != drawable.texture.width) ||
            (Metal_State.depth_texture.height != drawable.texture.height))
        {
          MTLTextureDescriptor *depth_desc = [[MTLTextureDescriptor alloc] init];
          depth_desc.pixelFormat = MTLPixelFormatDepth32Float;
          depth_desc.width = drawable.texture.width;
          depth_desc.height = drawable.texture.height;
          depth_desc.usage = MTLTextureUsageRenderTarget;
          depth_desc.storageMode = MTLStorageModePrivate;

          Metal_State.depth_texture = [Metal_State.device newTextureWithDescriptor:depth_desc];
        }
        
        id<MTLCommandBuffer> command_buffer = [Metal_State.command_queue commandBuffer];

        MTLRenderPassDescriptor *render_pass        = [MTLRenderPassDescriptor renderPassDescriptor];
        render_pass.colorAttachments[0].texture     = drawable.texture;
        render_pass.colorAttachments[0].loadAction  = MTLLoadActionClear;
        render_pass.colorAttachments[0].clearColor  = MTLClearColorMake(.1f, .1f, .1f, 1);
        render_pass.colorAttachments[0].storeAction = MTLStoreActionStore;

        render_pass.depthAttachment.texture     = Metal_State.depth_texture;
        render_pass.depthAttachment.clearDepth  = 1.0;
        render_pass.depthAttachment.loadAction  = MTLLoadActionClear;
        render_pass.depthAttachment.storeAction = MTLStoreActionStore;
        
        id<MTLRenderCommandEncoder> encoder = [command_buffer renderCommandEncoderWithDescriptor:render_pass];
        encoder.label = @"draw-call";
        
        for(R_Command_Header *it = R_Commands.first; it; it = it->next) {
        
        switch (it->type) {
          case R_Command_Type_Draw: {
            R_Command_Draw *draw = (R_Command_Draw *)(((U08 *)it) + sizeof(R_Command_Header));

            Assert(draw->pipeline.id, "invalid pipeline");
            Metal_Pipeline* metal_pipeline = &Metal_State.pipelines[draw->pipeline.id - 1];
           
            Assert(draw->vertex_buffer.id, "invalid vertex buffer");
            Metal_Buffer *metal_vertex_buffer = &Metal_State.buffers[draw->vertex_buffer.id - 1];

            Assert(draw->index_buffer.id, "invalid index buffer");
            Metal_Buffer *metal_index_buffer = &Metal_State.buffers[draw->index_buffer.id - 1];

            // depth range [0, 1]


            [encoder setViewport:(MTLViewport) {  draw->viewport_region.min.x,
                                                  display_size.height - draw->viewport_region.max.y,
                                                  draw->viewport_region.max.x - draw->viewport_region.min.x,
                                                  draw->viewport_region.max.y - draw->viewport_region.min.y,
                                                  0.0,
                                                  1.0 }];

            [encoder setRenderPipelineState:metal_pipeline->pipeline];

            if (draw->clip_region.max.x > display_size.width) {
              draw->clip_region.max.x = display_size.width;
            }

            if (draw->clip_region.max.y > display_size.height) {
              draw->clip_region.max.y = display_size.height;
            }

            MTLScissorRect scissor_rect = {
              .x        = draw->clip_region.min.x,
              .y        = display_size.height - draw->clip_region.max.y,
              .width    = draw->clip_region.max.x - draw->clip_region.min.x,
              .height   = draw->clip_region.max.y - draw->clip_region.min.y,
            };

            [encoder setScissorRect:scissor_rect];

            // NOTE(cmat): Depth testing.
            if (draw->depth_testing) {
              [encoder setDepthStencilState:Metal_State.depth_stencil_state];
            } else {
              [encoder setDepthStencilState:Metal_State.depth_stencil_none_state];
            }
            
            // NOTE(cmat): Set constant buffers.
            For_U32(it, draw->constant_buffer_count) {
              Assert(draw->constant_buffers[it].id, "invalid constant buffer");
              Metal_Buffer *metal_constant_buffer = &Metal_State.buffers[draw->constant_buffers[it].id - 1];
              [encoder setVertexBuffer:metal_constant_buffer->buffer offset:0 atIndex:it];
            }

            // NOTE(cmat): Set vertex buffer.
            [encoder setVertexBuffer:metal_vertex_buffer->buffer offset:0 atIndex:draw->constant_buffer_count];
            
            For_U32(it, R_Texture_Slots) {
              Assert(draw->texture_slots[it].id, "invalid texture");
              Metal_Texture *metal_texture = &Metal_State.textures[draw->texture_slots[it].id - 1];
              [encoder setFragmentTexture:(id<MTLTexture>)metal_texture->texture atIndex:it];
            }

            Assert(draw->sampler.id, "invalid sampler");
            Metal_Sampler *metal_sampler = &Metal_State.samplers[draw->sampler.id - 1];
            [encoder setFragmentSamplerState:(id<MTLSamplerState>)metal_sampler->sampler atIndex:0];

            [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                indexCount:draw->index_count
                                 indexType:MTLIndexTypeUInt32
                               indexBuffer:metal_index_buffer->buffer
                         indexBufferOffset:(draw->index_buffer_offset * sizeof(U32))];
              
            } break;

            Invalid_Default;
        }
      }

      [encoder endEncoding];
        
      [command_buffer presentDrawable:drawable];
      [command_buffer commit];
    }
  }
  
  r_command_reset();
}
