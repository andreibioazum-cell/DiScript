static int ds_vk_acquire_image(void) {
    DsVkFrame *f = &vk_frames[vk_frame_cur];
    if (vk_swap == VK_NULL_HANDLE)
        return 0;
    VkResult r = vkAcquireNextImageKHR(vk_device, vk_swap, DS_VK_ACQUIRE_TIMEOUT_NS, f->image_available, VK_NULL_HANDLE,
                                       &vk_sw_index);
    if (r == VK_ERROR_OUT_OF_DATE_KHR) {

        if (!ds_vk_recreate_swapchain())
            return 0;
        f = &vk_frames[vk_frame_cur];
        r = vkAcquireNextImageKHR(vk_device, vk_swap, DS_VK_ACQUIRE_TIMEOUT_NS, f->image_available, VK_NULL_HANDLE,
                                  &vk_sw_index);
    }
    if (r == VK_TIMEOUT || r == VK_NOT_READY) {

        return 0;
    }
    if (r == VK_ERROR_SURFACE_LOST_KHR) {
        vk_surface_lost = 1;
        ds_log_err("vulkan: surface lost on acquire");
        return 0;
    }
    if (r == VK_ERROR_DEVICE_LOST) {
        vk_device_lost = 1;
        ds_log_err("vulkan: device lost on acquire");
        return 0;
    }
    if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) {
        if (r == VK_ERROR_OUT_OF_DATE_KHR)
            vk_need_recreate = 1;
        else
            ds_log_err("vulkan: vkAcquireNextImageKHR failed (%d)", (int)r);
        return 0;
    }

    if (r == VK_SUBOPTIMAL_KHR)
        vk_sw_stale = 1;
    vk_image_held = 1;
    return 1;
}

static int ds_vk_begin_frame_backend(uint32_t w, uint32_t h, uint32_t ow, uint32_t oh) {

    if ((vk_surface_lost || vk_device_lost || (!vk_ready && vk_rebuild_window)) && !ds_vk_recover())
        return 0;
    if (!vk_ready || vk_surface == VK_NULL_HANDLE)
        return 0;
    if (w == 0 || h == 0 || ow == 0 || oh == 0)
        return 0;
    vk_log_w = w;
    vk_log_h = h;

    if (vk_need_recreate || ds_vk_swapchain_stale()) {
        if (!ds_vk_recreate_swapchain())
            return 0;
    }

    if (vk_sw_format != vk_rp_format && !ds_vk_rebuild_pipelines())
        return 0;

    ds_vk_process_pending_uploads();
    if (!ds_vk_ensure_offscreen(ow, oh, vk_sw_format))
        return 0;

    if (!vk_begun) {
        vk_frame_cur = (vk_frame_cur + 1) % DS_VK_FRAMES_IN_FLIGHT;
        vk_begun = 1;
    }
    DsVkFrame *f = &vk_frames[vk_frame_cur];
    /* Wait for prior submissions sharing the single offscreen render target. */
    for (int i = 0; i < DS_VK_FRAMES_IN_FLIGHT; i++) {
        DsVkFrame *o = &vk_frames[i];
        if (o == f || !o->submitted)
            continue;
        VkResult owr = vkWaitForFences(vk_device, 1, &o->fence, VK_TRUE, UINT64_MAX);
        if (owr == VK_ERROR_DEVICE_LOST)
            vk_device_lost = 1;
        if (!ds_vk_check_result(owr, "vkWaitForFences(frame)"))
            return 0;
        o->submitted = 0;
    }
    if (f->submitted) {
        VkResult wr = vkWaitForFences(vk_device, 1, &f->fence, VK_TRUE, UINT64_MAX);
        if (wr == VK_ERROR_DEVICE_LOST)
            vk_device_lost = 1;
        if (!ds_vk_check_result(wr, "vkWaitForFences(frame)"))
            return 0;
        f->submitted = 0;
    }
    if (!ds_vk_check_result(vkResetCommandBuffer(f->cmd, 0), "vkResetCommandBuffer(frame)"))
        return 0;

    if (!vk_image_held && !ds_vk_acquire_image())
        return 0;

    VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    if (!ds_vk_check_result(vkBeginCommandBuffer(f->cmd, &bi), "vkBeginCommandBuffer(frame)"))
        return 0;
    VkClearValue clear;
    /* Clear offscreen target with opaque alpha. */
    clear.color = (VkClearColorValue){{0.0f, 0.0f, 0.0f, 1.0f}};
    VkRenderPassBeginInfo ri = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    ri.renderPass = vk_renderpass;
    ri.framebuffer = vk_off_fb;
    ri.renderArea.extent.width = ow;
    ri.renderArea.extent.height = oh;
    ri.clearValueCount = 1;
    ri.pClearValues = &clear;
    vkCmdBeginRenderPass(f->cmd, &ri, VK_SUBPASS_CONTENTS_INLINE);
    VkViewport vp = {0.0f, 0.0f, (float)ow, (float)oh, 0.0f, 1.0f};
    VkRect2D sc = {{0, 0}, {ow, oh}};
    vkCmdSetViewport(f->cmd, 0, 1, &vp);
    vkCmdSetScissor(f->cmd, 0, 1, &sc);
    return 1;
}

static void ds_vk_end_frame_backend(void) {
    DsVkFrame *f = &vk_frames[vk_frame_cur];
    if (ds_vk_build_geometry() && geo_in > 0 && ds_vk_frame_ensure_capacity(f, geo_vn, geo_in)) {
        memcpy(f->vmap, geo_verts, geo_vn * sizeof(GeoVert));
        memcpy(f->imap, geo_idx, geo_in * sizeof(uint32_t));

        VkDeviceSize zero = 0;
        vkCmdBindVertexBuffers(f->cmd, 0, 1, &f->vbuf, &zero);
        vkCmdBindIndexBuffer(f->cmd, f->ibuf, 0, VK_INDEX_TYPE_UINT32);
        ds_vk_record_draws(f->cmd);
    }
    vkCmdEndRenderPass(f->cmd);
    if (vk_swap == VK_NULL_HANDLE || vk_sw_index >= vk_sw_count || vk_off_image == VK_NULL_HANDLE)
        return;
    ds_vk_record_present_blit(f->cmd);
    if (!ds_vk_check_result(vkEndCommandBuffer(f->cmd), "vkEndCommandBuffer(frame)"))
        return;
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &f->image_available;
    si.pWaitDstStageMask = &wait_stage;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &f->cmd;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &vk_sw_present[vk_sw_index];

    if (!ds_vk_check_result(vkResetFences(vk_device, 1, &f->fence), "vkResetFences(frame)"))
        return;
    VkResult sr = vkQueueSubmit(vk_queue, 1, &si, f->fence);
    if (sr == VK_ERROR_DEVICE_LOST)
        vk_device_lost = 1;
    if (!ds_vk_check_result(sr, "vkQueueSubmit(frame)"))
        return;
    f->submitted = 1;
    VkPresentInfoKHR pi = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &vk_sw_present[vk_sw_index];
    pi.swapchainCount = 1;
    pi.pSwapchains = &vk_swap;
    pi.pImageIndices = &vk_sw_index;
    VkResult pr = vkQueuePresentKHR(vk_queue, &pi);
    if (pr == VK_ERROR_OUT_OF_DATE_KHR)
        vk_need_recreate = 1;
    else if (pr == VK_SUBOPTIMAL_KHR)
        vk_sw_stale = 1;
    else if (pr == VK_ERROR_SURFACE_LOST_KHR) {
        vk_surface_lost = 1;
        ds_log_err("vulkan: surface lost on present");
    } else if (pr == VK_ERROR_DEVICE_LOST) {
        vk_device_lost = 1;
        ds_log_err("vulkan: device lost on present");
    } else if (pr != VK_SUCCESS)
        ds_log_err("vulkan: vkQueuePresentKHR failed (%d)", (int)pr);
    vk_image_held = 0;
    vk_begun = 0;
}
