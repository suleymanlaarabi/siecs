#include "render/render_internal.h"
#include "render/shaders.generated.h"
#include <stddef.h>
typedef struct {
    SDL_GPUDevice *device;
    SDL_GPUTextureFormat format;
    SDL_GPUGraphicsPipeline *rect_pipeline,*text_pipeline,*solid_text_pipeline;
    SDL_GPUSampler *sampler;
    SDL_GPUBuffer *rect_buffer,*text_buffer;
    SDL_GPUTransferBuffer *rect_transfer,*text_transfer;
    uint32_t rect_capacity,text_capacity;
    sigpu_overlay_id_t overlay_id;
} ui_renderer_t;
static ui_renderer_t renderer;
static SDL_GPUShader *shader(SDL_GPUDevice *device,const unsigned char *code,size_t size,
                             SDL_GPUShaderStage stage,uint32_t samplers,uint32_t uniforms) {
    SDL_GPUShaderCreateInfo info={.code_size=size,.code=code,.entrypoint="main",
        .format=SDL_GPU_SHADERFORMAT_SPIRV,.stage=stage,.num_samplers=samplers,
        .num_uniform_buffers=uniforms};
    return SDL_CreateGPUShader(device,&info);
}
static SDL_GPUGraphicsPipeline *pipeline(SDL_GPUDevice *device,SDL_GPUTextureFormat format,ui_draw_kind_t kind) {
    bool text=kind!=UI_DRAW_RECT;
    SDL_GPUShader *vertex=shader(device,text?siui_text_vert_spv:siui_rect_vert_spv,
        text?sizeof(siui_text_vert_spv):sizeof(siui_rect_vert_spv),SDL_GPU_SHADERSTAGE_VERTEX,0,1);
    const unsigned char *fragment_code=kind==UI_DRAW_RECT?siui_rect_frag_spv:
        kind==UI_DRAW_TEXT?siui_text_frag_spv:siui_solid_frag_spv;
    size_t fragment_size=kind==UI_DRAW_RECT?sizeof(siui_rect_frag_spv):
        kind==UI_DRAW_TEXT?sizeof(siui_text_frag_spv):sizeof(siui_solid_frag_spv);
    SDL_GPUShader *fragment=shader(device,fragment_code,fragment_size,SDL_GPU_SHADERSTAGE_FRAGMENT,
                                  kind==UI_DRAW_TEXT?1:0,0);
    if(!vertex || !fragment) {
        if(vertex) SDL_ReleaseGPUShader(device,vertex);
        if(fragment) SDL_ReleaseGPUShader(device,fragment);
        return NULL;
    }
    SDL_GPUVertexBufferDescription buffer={.slot=0,
        .pitch=text?sizeof(ui_text_vertex_t):sizeof(ui_rect_instance_t),
        .input_rate=text?SDL_GPU_VERTEXINPUTRATE_VERTEX:SDL_GPU_VERTEXINPUTRATE_INSTANCE};
    SDL_GPUVertexAttribute rect_attributes[]={
        {.location=0,.buffer_slot=0,.format=SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,.offset=offsetof(ui_rect_instance_t,x)},
        {.location=1,.buffer_slot=0,.format=SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM,.offset=offsetof(ui_rect_instance_t,background)},
        {.location=2,.buffer_slot=0,.format=SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,.offset=offsetof(ui_rect_instance_t,top)},
        {.location=3,.buffer_slot=0,.format=SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM,.offset=offsetof(ui_rect_instance_t,top_color)},
        {.location=4,.buffer_slot=0,.format=SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM,.offset=offsetof(ui_rect_instance_t,right_color)},
        {.location=5,.buffer_slot=0,.format=SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM,.offset=offsetof(ui_rect_instance_t,bottom_color)},
        {.location=6,.buffer_slot=0,.format=SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM,.offset=offsetof(ui_rect_instance_t,left_color)},
    };
    SDL_GPUVertexAttribute text_attributes[]={
        {.location=0,.buffer_slot=0,.format=SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,.offset=offsetof(ui_text_vertex_t,x)},
        {.location=1,.buffer_slot=0,.format=SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,.offset=offsetof(ui_text_vertex_t,u)},
        {.location=2,.buffer_slot=0,.format=SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM,.offset=offsetof(ui_text_vertex_t,color)},
        {.location=3,.buffer_slot=0,.format=SDL_GPU_VERTEXELEMENTFORMAT_FLOAT,.offset=offsetof(ui_text_vertex_t,image_type)},
    };
    SDL_GPUColorTargetDescription target={.format=format,.blend_state={
        .src_color_blendfactor=SDL_GPU_BLENDFACTOR_SRC_ALPHA,
        .dst_color_blendfactor=SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
        .color_blend_op=SDL_GPU_BLENDOP_ADD,
        .src_alpha_blendfactor=SDL_GPU_BLENDFACTOR_ONE,
        .dst_alpha_blendfactor=SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
        .alpha_blend_op=SDL_GPU_BLENDOP_ADD,
        .enable_blend=true,
    }};
    SDL_GPUGraphicsPipelineCreateInfo info={
        .vertex_shader=vertex,.fragment_shader=fragment,
        .vertex_input_state={.vertex_buffer_descriptions=&buffer,.num_vertex_buffers=1,
            .vertex_attributes=text?text_attributes:rect_attributes,
            .num_vertex_attributes=text?4:7},
        .primitive_type=SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .rasterizer_state={.fill_mode=SDL_GPU_FILLMODE_FILL,.cull_mode=SDL_GPU_CULLMODE_NONE},
        .multisample_state={.sample_count=SDL_GPU_SAMPLECOUNT_1},
        .target_info={.color_target_descriptions=&target,.num_color_targets=1},
    };
    SDL_GPUGraphicsPipeline *result=SDL_CreateGPUGraphicsPipeline(device,&info);
    SDL_ReleaseGPUShader(device,vertex);SDL_ReleaseGPUShader(device,fragment);
    return result;
}
static bool init_gpu(const sigpu_overlay_context_t *context) {
    if(renderer.device==context->device && renderer.format==context->format && renderer.rect_pipeline && renderer.text_pipeline && renderer.solid_text_pipeline)
        return true;
    if(renderer.device) {
        if(renderer.rect_pipeline) SDL_ReleaseGPUGraphicsPipeline(renderer.device,renderer.rect_pipeline);
        if(renderer.text_pipeline) SDL_ReleaseGPUGraphicsPipeline(renderer.device,renderer.text_pipeline);
        if(renderer.solid_text_pipeline) SDL_ReleaseGPUGraphicsPipeline(renderer.device,renderer.solid_text_pipeline);
        if(renderer.sampler) SDL_ReleaseGPUSampler(renderer.device,renderer.sampler);
        if(renderer.rect_buffer) SDL_ReleaseGPUBuffer(renderer.device,renderer.rect_buffer);
        if(renderer.text_buffer) SDL_ReleaseGPUBuffer(renderer.device,renderer.text_buffer);
        if(renderer.rect_transfer) SDL_ReleaseGPUTransferBuffer(renderer.device,renderer.rect_transfer);
        if(renderer.text_transfer) SDL_ReleaseGPUTransferBuffer(renderer.device,renderer.text_transfer);
        renderer.rect_capacity=renderer.text_capacity=0;
    }
    renderer.device=context->device;renderer.format=context->format;
    renderer.rect_pipeline=pipeline(context->device,context->format,UI_DRAW_RECT);
    renderer.text_pipeline=pipeline(context->device,context->format,UI_DRAW_TEXT);
    renderer.solid_text_pipeline=pipeline(context->device,context->format,UI_DRAW_SOLID_TEXT);
    SDL_GPUSamplerCreateInfo sampler={.min_filter=SDL_GPU_FILTER_LINEAR,.mag_filter=SDL_GPU_FILTER_LINEAR,
        .mipmap_mode=SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
        .address_mode_u=SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v=SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE};
    renderer.sampler=SDL_CreateGPUSampler(context->device,&sampler);
    ui_cache.paint_dirty=true;
    return renderer.rect_pipeline && renderer.text_pipeline && renderer.solid_text_pipeline && renderer.sampler;
}
static bool reserve_gpu(SDL_GPUDevice *device,SDL_GPUBuffer **buffer,SDL_GPUTransferBuffer **transfer,
                        uint32_t *capacity,uint32_t needed) {
    if(needed<=*capacity) return true;
    uint32_t size=*capacity?*capacity:4096;
    while(size<needed) size*=2;
    SDL_GPUBuffer *new_buffer=SDL_CreateGPUBuffer(device,&(SDL_GPUBufferCreateInfo){
        .usage=SDL_GPU_BUFFERUSAGE_VERTEX,.size=size});
    SDL_GPUTransferBuffer *new_transfer=SDL_CreateGPUTransferBuffer(device,&(SDL_GPUTransferBufferCreateInfo){
        .usage=SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,.size=size});
    if(!new_buffer || !new_transfer) {
        if(new_buffer) SDL_ReleaseGPUBuffer(device,new_buffer);
        if(new_transfer) SDL_ReleaseGPUTransferBuffer(device,new_transfer);
        return false;
    }
    if(*buffer) SDL_ReleaseGPUBuffer(device,*buffer);
    if(*transfer) SDL_ReleaseGPUTransferBuffer(device,*transfer);
    *buffer=new_buffer;*transfer=new_transfer;*capacity=size;
    return true;
}
static bool upload(const sigpu_overlay_context_t *context) {
    uint32_t rect_bytes=ui_batches.rect_count*sizeof(ui_rect_instance_t);
    uint32_t text_bytes=ui_batches.vertex_count*sizeof(ui_text_vertex_t);
    if(!rect_bytes && !text_bytes) return true;
    if(rect_bytes && !reserve_gpu(context->device,&renderer.rect_buffer,&renderer.rect_transfer,
                                  &renderer.rect_capacity,rect_bytes)) return false;
    if(text_bytes && !reserve_gpu(context->device,&renderer.text_buffer,&renderer.text_transfer,
                                  &renderer.text_capacity,text_bytes)) return false;
    uint64_t start=SDL_GetTicksNS();
    if(rect_bytes) {
        void *dst=SDL_MapGPUTransferBuffer(context->device,renderer.rect_transfer,true);
        if(!dst) return false;
        memcpy(dst,ui_batches.rects,rect_bytes);SDL_UnmapGPUTransferBuffer(context->device,renderer.rect_transfer);
    }
    if(text_bytes) {
        void *dst=SDL_MapGPUTransferBuffer(context->device,renderer.text_transfer,true);
        if(!dst) return false;
        memcpy(dst,ui_batches.vertices,text_bytes);SDL_UnmapGPUTransferBuffer(context->device,renderer.text_transfer);
    }
    SDL_GPUCopyPass *copy=SDL_BeginGPUCopyPass(context->command_buffer);
    if(copy) {
        if(rect_bytes) SDL_UploadToGPUBuffer(copy,&(SDL_GPUTransferBufferLocation){renderer.rect_transfer,0},
            &(SDL_GPUBufferRegion){renderer.rect_buffer,0,rect_bytes},true);
        if(text_bytes) SDL_UploadToGPUBuffer(copy,&(SDL_GPUTransferBufferLocation){renderer.text_transfer,0},
            &(SDL_GPUBufferRegion){renderer.text_buffer,0,text_bytes},true);
        SDL_EndGPUCopyPass(copy);
    } else return false;
    ui_cache.stats.gpu_upload_ns=SDL_GetTicksNS()-start;
    return true;
}
static void overlay(const sigpu_overlay_context_t *context,void *unused) {
    (void)unused;
    if(!context->target || !context->command_buffer || !init_gpu(context)) return;
    siui_layout_update((float)context->width,(float)context->height);
    ui_text_gpu_engine(context->device);
    bool changed=ui_cache.paint_dirty;
    ui_collect(context);
    if(changed && !upload(context)) {ui_cache.paint_dirty=true;return;}
    if(!ui_batches.command_count) return;
    SDL_GPUColorTargetInfo target={.texture=context->target,.load_op=SDL_GPU_LOADOP_LOAD,
                                   .store_op=SDL_GPU_STOREOP_STORE};
    SDL_GPURenderPass *pass=SDL_BeginGPURenderPass(context->command_buffer,&target,1,NULL);
    if(!pass) return;
    float viewport[4]={(float)context->width,(float)context->height,0,0};
    SDL_PushGPUVertexUniformData(context->command_buffer,0,viewport,sizeof(viewport));
    ui_draw_kind_t bound=(ui_draw_kind_t)-1;
    for(uint32_t i=0;i<ui_batches.command_count;i++) {
        const ui_draw_command_t *draw=&ui_batches.commands[i];
        if(draw->kind!=bound) {
            bound=draw->kind;
            SDL_BindGPUGraphicsPipeline(pass,bound==UI_DRAW_RECT?renderer.rect_pipeline:
                bound==UI_DRAW_TEXT?renderer.text_pipeline:renderer.solid_text_pipeline);
            SDL_GPUBufferBinding binding={bound==UI_DRAW_RECT?renderer.rect_buffer:renderer.text_buffer,0};
            SDL_BindGPUVertexBuffers(pass,0,&binding,1);
        }
        SDL_SetGPUScissor(pass,&draw->clip);
        if(bound==UI_DRAW_TEXT) {
            SDL_GPUTextureSamplerBinding sampler={draw->atlas,renderer.sampler};
            SDL_BindGPUFragmentSamplers(pass,0,&sampler,1);
            SDL_DrawGPUPrimitives(pass,draw->count,1,draw->first,0);
        } else SDL_DrawGPUPrimitives(pass,6,draw->count,0,draw->first);
        ui_cache.stats.draw_calls++;
    }
    SDL_EndGPURenderPass(pass);
}
void ui_render_register(void) {renderer.overlay_id=sigpu_overlay_register(overlay,NULL);}
void ui_render_fini(void) {
    if(renderer.overlay_id) sigpu_overlay_unregister(renderer.overlay_id);
    ui_batches_fini();
    if(renderer.device) {
        if(renderer.rect_pipeline) SDL_ReleaseGPUGraphicsPipeline(renderer.device,renderer.rect_pipeline);
        if(renderer.text_pipeline) SDL_ReleaseGPUGraphicsPipeline(renderer.device,renderer.text_pipeline);
        if(renderer.solid_text_pipeline) SDL_ReleaseGPUGraphicsPipeline(renderer.device,renderer.solid_text_pipeline);
        if(renderer.sampler) SDL_ReleaseGPUSampler(renderer.device,renderer.sampler);
        if(renderer.rect_buffer) SDL_ReleaseGPUBuffer(renderer.device,renderer.rect_buffer);
        if(renderer.text_buffer) SDL_ReleaseGPUBuffer(renderer.device,renderer.text_buffer);
        if(renderer.rect_transfer) SDL_ReleaseGPUTransferBuffer(renderer.device,renderer.rect_transfer);
        if(renderer.text_transfer) SDL_ReleaseGPUTransferBuffer(renderer.device,renderer.text_transfer);
    }
    renderer=(ui_renderer_t){0};
}
