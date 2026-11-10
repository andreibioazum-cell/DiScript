static int ds_vk_batch_push(int pipeline, const void *desc, size_t first, size_t count, int sx, int sy, int sw,
                            int sh) {
    if (count == 0)
        return 1;
    if (vk_batch_n == vk_batch_cap) {
        size_t cap = vk_batch_cap ? vk_batch_cap * 2 : 256;
        if (cap > SIZE_MAX / sizeof(GeoBatch))
            return 0;
        GeoBatch *nb = (GeoBatch *)realloc(vk_batches, cap * sizeof(*nb));
        if (!nb) {
            ds_log_err("out of memory for draw batches");
            return 0;
        }
        vk_batches = nb;
        vk_batch_cap = cap;
    }
    GeoBatch *b = &vk_batches[vk_batch_n++];
    b->pipeline = pipeline;
    b->desc = desc;
    b->first = (uint32_t)first;
    b->count = (uint32_t)count;
    b->sx = sx;
    b->sy = sy;
    b->sw = sw;
    b->sh = sh;
    return 1;
}

static int ds_vk_build_geometry(void) {
    uint32_t lw = vk_log_w ? vk_log_w : vk_off_w, lh = vk_log_h ? vk_log_h : vk_off_h;
    int sx = 0, sy = 0, sw = (int)vk_off_w, sh = (int)vk_off_h, clip_empty = 0;
    ds_vk_process_pending_uploads();
    geo_reset();
    vk_batch_n = 0;
    const void *font_desc = vk_font_desc ? (const void *)(uintptr_t)vk_font_desc : NULL;
    for (size_t i = 0; i < cmd_n; i++) {
        DSCmd *c = &cmds[i];
        int pipeline = 0;
        const void *desc = NULL;
        size_t is = geo_in;
        if (c->t == DS_CMD_SCISSOR) {
            if (c->v.rc.w <= 0.0f || c->v.rc.h <= 0.0f || lw == 0 || lh == 0) {
                sx = 0;
                sy = 0;
                sw = (int)vk_off_w;
                sh = (int)vk_off_h;
                clip_empty = 0;
            } else {
                float scale_x = (float)vk_off_w / (float)lw;
                float scale_y = (float)vk_off_h / (float)lh;
                int x0 = (int)floorf(c->v.rc.x * scale_x);
                int y0 = (int)floorf(c->v.rc.y * scale_y);
                int x1 = (int)ceilf((c->v.rc.x + c->v.rc.w) * scale_x);
                int y1 = (int)ceilf((c->v.rc.y + c->v.rc.h) * scale_y);
                if (x0 < 0) x0 = 0;
                if (y0 < 0) y0 = 0;
                if (x1 > (int)vk_off_w) x1 = (int)vk_off_w;
                if (y1 > (int)vk_off_h) y1 = (int)vk_off_h;
                if (x1 <= x0 || y1 <= y0) {
                    clip_empty = 1;
                } else {
                    sx = x0;
                    sy = y0;
                    sw = x1 - x0;
                    sh = y1 - y0;
                    clip_empty = 0;
                }
            }
            continue;
        }
        if (clip_empty)
            continue;
        switch (c->t) {
        case DS_CMD_RECT:
            geo_rect(c->v.rc.x, c->v.rc.y, c->v.rc.w, c->v.rc.h, c->v.rc.c);
            break;
        case DS_CMD_ROUND:

            pipeline = 0;
            desc = NULL;
            geo_roundrect(c->v.rr.x, c->v.rr.y, c->v.rr.w, c->v.rr.h, c->v.rr.r, c->v.rr.c);
            break;
        case DS_CMD_RECT_ROT:

            geo_rect_rot(c->v.rot.x, c->v.rot.y, c->v.rot.w, c->v.rot.h, c->v.rot.ang, c->v.rot.c);
            break;
        case DS_CMD_CIRCLE:
            geo_circle(c->v.ci.x, c->v.ci.y, c->v.ci.r, c->v.ci.c);
            break;
        case DS_CMD_SECTOR:
            geo_sector(c->v.sec.x, c->v.sec.y, c->v.sec.r, c->v.sec.start_deg,
                       c->v.sec.sweep_deg, c->v.sec.c);
            break;
        case DS_CMD_RING:
            geo_ring(c->v.rg.x, c->v.rg.y, c->v.rg.r, c->v.rg.th, c->v.rg.c);
            break;
        case DS_CMD_LINE:
            geo_line(c->v.ln.x1, c->v.ln.y1, c->v.ln.x2, c->v.ln.y2, c->v.ln.th, c->v.ln.c);
            break;
        case DS_CMD_TEX: {
            Texture *t = c->v.tx.tx;
            if (t && t->gpu.uploaded && t->gpu.desc) {
                pipeline = 1;
                desc = (const void *)(uintptr_t)t->gpu.desc;
                geo_tex(c->v.tx.x, c->v.tx.y, c->v.tx.a, c->v.tx.sc, (float)t->w, (float)t->h, 0xffffffffu);
            }
            break;
        }
        case DS_CMD_TEX_TINT: {
            Texture *t = c->v.tx2.tx;
            if (t && t->gpu.uploaded && t->gpu.desc) {

                pipeline = 2;
                desc = (const void *)(uintptr_t)t->gpu.desc;
                geo_tex(c->v.tx2.x, c->v.tx2.y, c->v.tx2.a, c->v.tx2.sc, (float)t->w, (float)t->h, c->v.tx2.c);
            }
            break;
        }
        case DS_CMD_TEX_FADE: {
            Texture *t = c->v.tx2.tx;
            if (t && t->gpu.uploaded && t->gpu.desc) {
                pipeline = 1;
                desc = (const void *)(uintptr_t)t->gpu.desc;
                geo_tex(c->v.tx2.x, c->v.tx2.y, c->v.tx2.a, c->v.tx2.sc, (float)t->w, (float)t->h, c->v.tx2.c);
            }
            break;
        }
        case DS_CMD_TEX_ROUND: {
            Texture *t = c->v.txr.tx;
            if (t && t->gpu.uploaded && t->gpu.desc) {
                pipeline = 1;
                desc = (const void *)(uintptr_t)t->gpu.desc;
                geo_tex_round(c->v.txr.x, c->v.txr.y, c->v.txr.sc, (float)t->w, (float)t->h, c->v.txr.r,
                              0xffffffffu);
            }
            break;
        }
        case DS_CMD_TEXT:
            if (font && font_desc) {
                pipeline = 1;
                desc = font_desc;
                geo_text(c->v.tt.s, c->v.tt.x, c->v.tt.y, c->v.tt.c, c->v.tt.sc, font);
            }
            break;
        case DS_CMD_TEXT_RAINBOW:
            if (font && font_desc) {
                pipeline = 1;
                desc = font_desc;
                if (c->v.tr.c)
                    geo_text_shimmer(c->v.tr.s, c->v.tr.x, c->v.tr.y, c->v.tr.c, c->v.tr.sc, c->v.tr.hue, font);
                else
                    geo_text_rainbow(c->v.tr.s, c->v.tr.x, c->v.tr.y, c->v.tr.sc, c->v.tr.hue, font);
            }
            break;
        case DS_CMD_SCISSOR:
            break;
        }
        if (!ds_vk_batch_push(pipeline, desc, is, geo_in - is, sx, sy, sw, sh))
            return 0;
    }
    return 1;
}

static void ds_vk_record_draws(VkCommandBuffer cmd) {
    if (vk_off_w == 0 || vk_off_h == 0)
        return;

    uint32_t lw = vk_log_w ? vk_log_w : vk_off_w, lh = vk_log_h ? vk_log_h : vk_off_h;
    float pc[4] = {2.0f / (float)lw, 2.0f / (float)lh, -1.0f, -1.0f};
    int cur_pipe = -1;
    const void *cur_desc = NULL;
    int cur_sx = 0, cur_sy = 0, cur_sw = (int)vk_off_w, cur_sh = (int)vk_off_h;
    size_t i = 0;
    while (i < vk_batch_n) {
        int pipe = vk_batches[i].pipeline;
        const void *desc = (pipe == 1 || pipe == 2) ? vk_batches[i].desc : NULL;
        int bsx = vk_batches[i].sx, bsy = vk_batches[i].sy, bsw = vk_batches[i].sw, bsh = vk_batches[i].sh;
        uint32_t first = vk_batches[i].first, count = 0;
        size_t j = i;
        while (j < vk_batch_n && vk_batches[j].pipeline == pipe && (desc == NULL || vk_batches[j].desc == desc) &&
               vk_batches[j].sx == bsx && vk_batches[j].sy == bsy && vk_batches[j].sw == bsw &&
               vk_batches[j].sh == bsh) {
            count += vk_batches[j].count;
            j++;
        }
        if (bsx != cur_sx || bsy != cur_sy || bsw != cur_sw || bsh != cur_sh) {
            VkRect2D sc = {{bsx, bsy}, {(uint32_t)bsw, (uint32_t)bsh}};
            vkCmdSetScissor(cmd, 0, 1, &sc);
            cur_sx = bsx;
            cur_sy = bsy;
            cur_sw = bsw;
            cur_sh = bsh;
        }
        if (pipe != cur_pipe) {
            VkPipelineLayout layout = pipe == 0   ? vk_layout_solid
                                      : pipe == 1 ? vk_layout_image
                                      : pipe == 2 ? vk_layout_tint
                                                  : vk_layout_solid;
            VkPipeline pipeline = pipe == 0   ? vk_pipe_solid
                                  : pipe == 1 ? vk_pipe_image
                                  : pipe == 2 ? vk_pipe_tint
                                              : vk_pipe_replace;
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), pc);
            cur_pipe = pipe;
            cur_desc = NULL;
        }
        if (desc && desc != cur_desc) {
            VkDescriptorSet set = (VkDescriptorSet)(uintptr_t)desc;
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe == 1 ? vk_layout_image : vk_layout_tint,
                                    0, 1, &set, 0, NULL);
            cur_desc = desc;
        }
        vkCmdDrawIndexed(cmd, count, 1, first, 0, 0);
        i = j;
    }
}

static int ds_vk_create_renderpass(void) {
    VkAttachmentDescription atts[2] = {0};
    VkAttachmentReference color = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference resolve = {1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub = {0};
    VkSubpassDependency dep = {0};
    VkRenderPassCreateInfo ri = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};

    atts[0].format = vk_sw_format;
    atts[0].samples = vk_msaa_samples;
    atts[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    atts[0].storeOp = vk_msaa_samples == VK_SAMPLE_COUNT_1_BIT ? VK_ATTACHMENT_STORE_OP_STORE
                                                                 : VK_ATTACHMENT_STORE_OP_DONT_CARE;
    atts[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    atts[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    atts[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    atts[0].finalLayout = vk_msaa_samples == VK_SAMPLE_COUNT_1_BIT
                              ? (ds_vk_smaa_enabled() ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                                      : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
                              : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &color;
    ri.attachmentCount = 1;
    ri.pAttachments = atts;
    if (vk_msaa_samples != VK_SAMPLE_COUNT_1_BIT) {
        atts[1].format = vk_sw_format;
        atts[1].samples = VK_SAMPLE_COUNT_1_BIT;
        atts[1].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        atts[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        atts[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        atts[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        atts[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        atts[1].finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        sub.pResolveAttachments = &resolve;
        ri.attachmentCount = 2;
    }
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dep.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    ri.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    ri.subpassCount = 1;
    ri.pSubpasses = &sub;
    ri.dependencyCount = 1;
    ri.pDependencies = &dep;
    int ok = ds_vk_check_result(vkCreateRenderPass(vk_device, &ri, NULL, &vk_renderpass), "vkCreateRenderPass");
    if (ok)
        vk_rp_format = vk_sw_format;
    return ok;
}

static int ds_vk_create_smaa_renderpass(void) {
    if (!ds_vk_smaa_enabled())
        return 1;
    VkAttachmentDescription att = {0};
    att.format = vk_sw_format;
    att.samples = VK_SAMPLE_COUNT_1_BIT;
    att.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    att.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    VkAttachmentReference ref = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub = {0};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &ref;
    VkSubpassDependency dep = {0};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dep.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    VkRenderPassCreateInfo ri = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    ri.attachmentCount = 1;
    ri.pAttachments = &att;
    ri.subpassCount = 1;
    ri.pSubpasses = &sub;
    ri.dependencyCount = 1;
    ri.pDependencies = &dep;
    return ds_vk_check_result(vkCreateRenderPass(vk_device, &ri, NULL, &vk_smaa_renderpass),
                              "vkCreateRenderPass(smaa)");
}

static void ds_vk_destroy_smaa_renderpass(void) {
    if (vk_device && vk_smaa_renderpass != VK_NULL_HANDLE)
        vkDestroyRenderPass(vk_device, vk_smaa_renderpass, NULL);
    vk_smaa_renderpass = VK_NULL_HANDLE;
}

static void ds_vk_destroy_all_pipelines(void) {
    if (!vk_device)
        return;
    if (vk_pipe_solid != VK_NULL_HANDLE)
        vkDestroyPipeline(vk_device, vk_pipe_solid, NULL);
    if (vk_pipe_replace != VK_NULL_HANDLE)
        vkDestroyPipeline(vk_device, vk_pipe_replace, NULL);
    if (vk_pipe_image != VK_NULL_HANDLE)
        vkDestroyPipeline(vk_device, vk_pipe_image, NULL);
    if (vk_pipe_tint != VK_NULL_HANDLE)
        vkDestroyPipeline(vk_device, vk_pipe_tint, NULL);
    if (vk_pipe_smaa != VK_NULL_HANDLE)
        vkDestroyPipeline(vk_device, vk_pipe_smaa, NULL);
    vk_pipe_solid = vk_pipe_replace = vk_pipe_image = vk_pipe_tint = vk_pipe_smaa = VK_NULL_HANDLE;
}

static int ds_vk_create_all_pipelines(void) {
    VkShaderModule frag_solid = VK_NULL_HANDLE, frag_image = VK_NULL_HANDLE, frag_tint = VK_NULL_HANDLE;
    VkShaderModule frag_smaa = VK_NULL_HANDLE;
    int ok =
        ds_vk_create_shader_module(ds_spirv_solid_frag, sizeof(ds_spirv_solid_frag) / sizeof(uint32_t), &frag_solid) &&
        ds_vk_create_shader_module(ds_spirv_image_frag, sizeof(ds_spirv_image_frag) / sizeof(uint32_t), &frag_image) &&
        ds_vk_create_shader_module(ds_spirv_tint_frag, sizeof(ds_spirv_tint_frag) / sizeof(uint32_t), &frag_tint) &&
        ds_vk_create_pipeline(frag_solid, vk_layout_solid, 1, vk_renderpass, vk_msaa_samples, &vk_pipe_solid) &&
        ds_vk_create_pipeline(frag_solid, vk_layout_solid, 0, vk_renderpass, vk_msaa_samples, &vk_pipe_replace) &&
        ds_vk_create_pipeline(frag_image, vk_layout_image, 1, vk_renderpass, vk_msaa_samples, &vk_pipe_image) &&
        ds_vk_create_pipeline(frag_tint, vk_layout_tint, 1, vk_renderpass, vk_msaa_samples, &vk_pipe_tint);
    if (ok && ds_vk_smaa_enabled()) {
        ok = ds_vk_create_shader_module(ds_spirv_smaa_frag, sizeof(ds_spirv_smaa_frag) / sizeof(uint32_t), &frag_smaa) &&
             ds_vk_create_pipeline(frag_smaa, vk_layout_image, 0, vk_smaa_renderpass, VK_SAMPLE_COUNT_1_BIT,
                                   &vk_pipe_smaa);
    }
    if (frag_solid != VK_NULL_HANDLE)
        vkDestroyShaderModule(vk_device, frag_solid, NULL);
    if (frag_image != VK_NULL_HANDLE)
        vkDestroyShaderModule(vk_device, frag_image, NULL);
    if (frag_tint != VK_NULL_HANDLE)
        vkDestroyShaderModule(vk_device, frag_tint, NULL);
    if (frag_smaa != VK_NULL_HANDLE)
        vkDestroyShaderModule(vk_device, frag_smaa, NULL);
    return ok;
}

static int ds_vk_rebuild_pipelines(void) {
    if (vk_device)
        vkDeviceWaitIdle(vk_device);
    ds_vk_destroy_offscreen();
    ds_vk_destroy_all_pipelines();
    if (vk_renderpass != VK_NULL_HANDLE)
        vkDestroyRenderPass(vk_device, vk_renderpass, NULL);
    vk_renderpass = VK_NULL_HANDLE;
    ds_vk_destroy_smaa_renderpass();
    if (!ds_vk_create_renderpass())
        return 0;
    if (!ds_vk_create_smaa_renderpass())
        return 0;
    return ds_vk_create_all_pipelines();
}

static void ds_vk_record_smaa(VkCommandBuffer cmd, DsVkFrame *f, uint32_t first_index) {
    if (!ds_vk_smaa_enabled() || vk_smaa_fb == VK_NULL_HANDLE || vk_smaa_desc == VK_NULL_HANDLE ||
        vk_pipe_smaa == VK_NULL_HANDLE)
        return;
    VkImageMemoryBarrier source_ready = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    source_ready.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    source_ready.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    source_ready.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    source_ready.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    source_ready.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    source_ready.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    source_ready.image = vk_off_image;
    source_ready.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    source_ready.subresourceRange.levelCount = 1;
    source_ready.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                         NULL, 0, NULL, 1, &source_ready);

    VkClearValue clear;
    clear.color = (VkClearColorValue){{0.0f, 0.0f, 0.0f, 1.0f}};
    VkRenderPassBeginInfo ri = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    ri.renderPass = vk_smaa_renderpass;
    ri.framebuffer = vk_smaa_fb;
    ri.renderArea.extent.width = vk_off_w;
    ri.renderArea.extent.height = vk_off_h;
    ri.clearValueCount = 1;
    ri.pClearValues = &clear;
    vkCmdBeginRenderPass(cmd, &ri, VK_SUBPASS_CONTENTS_INLINE);
    VkViewport vp = {0.0f, 0.0f, (float)vk_off_w, (float)vk_off_h, 0.0f, 1.0f};
    VkRect2D sc = {{0, 0}, {vk_off_w, vk_off_h}};
    VkDeviceSize zero = 0;
    float pc[4] = {2.0f / (float)vk_off_w, 2.0f / (float)vk_off_h, -1.0f, -1.0f};
    vkCmdSetViewport(cmd, 0, 1, &vp);
    vkCmdSetScissor(cmd, 0, 1, &sc);
    vkCmdBindVertexBuffers(cmd, 0, 1, &f->vbuf, &zero);
    vkCmdBindIndexBuffer(cmd, f->ibuf, 0, VK_INDEX_TYPE_UINT16);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipe_smaa);
    vkCmdPushConstants(cmd, vk_layout_image, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), pc);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_layout_image, 0, 1, &vk_smaa_desc, 0, NULL);
    vkCmdDrawIndexed(cmd, 6, 1, first_index, 0, 0);
    vkCmdEndRenderPass(cmd);
}

static void ds_vk_record_present_blit(VkCommandBuffer cmd) {
    VkImage sw_image = vk_sw_images[vk_sw_index];
    VkImage source = ds_vk_smaa_enabled() ? vk_smaa_image : vk_off_image;
    VkImageMemoryBarrier to_dst = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    to_dst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_dst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_dst.image = sw_image;
    to_dst.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    to_dst.subresourceRange.levelCount = 1;
    to_dst.subresourceRange.layerCount = 1;
    to_dst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    to_dst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_dst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1,
                         &to_dst);
    if (vk_blit_ok) {
        VkImageBlit bl;
        memset(&bl, 0, sizeof(bl));
        bl.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        bl.srcSubresource.layerCount = 1;
        bl.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        bl.dstSubresource.layerCount = 1;
        bl.srcOffsets[1].x = (int32_t)vk_off_w;
        bl.srcOffsets[1].y = (int32_t)vk_off_h;
        bl.srcOffsets[1].z = 1;
        bl.dstOffsets[1].x = (int32_t)vk_sw_extent.width;
        bl.dstOffsets[1].y = (int32_t)vk_sw_extent.height;
        bl.dstOffsets[1].z = 1;
        /* At native 1:1 there is nothing to resample. Keep it exact; the
         * selected MSAA/SMAA path has already smoothed the rendered scene.
         * A resized smooth frame uses linear sampling instead. */
        int scaled = vk_off_w != vk_sw_extent.width || vk_off_h != vk_sw_extent.height;
        VkFilter filter = vk_render_mode != 0 && scaled ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
        vkCmdBlitImage(cmd, source, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, sw_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &bl, filter);
    } else {
        VkImageCopy cp;
        memset(&cp, 0, sizeof(cp));
        cp.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        cp.srcSubresource.layerCount = 1;
        cp.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        cp.dstSubresource.layerCount = 1;
        cp.extent.width = vk_off_w < vk_sw_extent.width ? vk_off_w : vk_sw_extent.width;
        cp.extent.height = vk_off_h < vk_sw_extent.height ? vk_off_h : vk_sw_extent.height;
        cp.extent.depth = 1;
        vkCmdCopyImage(cmd, source, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, sw_image,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cp);
    }
    VkImageMemoryBarrier to_present = to_dst;
    to_present.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    to_present.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    to_present.dstAccessMask = 0;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL,
                         1, &to_present);
}
