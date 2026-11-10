static int ds_vk_create_shader_module(const uint32_t *code, size_t words, VkShaderModule *out) {
    VkShaderModuleCreateInfo ci = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ci.codeSize = words * sizeof(uint32_t);
    ci.pCode = code;
    return ds_vk_check_result(vkCreateShaderModule(vk_device, &ci, NULL, out), "vkCreateShaderModule");
}

static int ds_vk_create_pipeline(VkShaderModule frag, VkPipelineLayout layout, int blend, VkRenderPass renderpass,
                                 VkSampleCountFlagBits samples, VkPipeline *out) {
    VkPipelineShaderStageCreateInfo stages[2] = {0};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = VK_NULL_HANDLE;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName = "main";

    *out = VK_NULL_HANDLE;
    if (!ds_vk_create_shader_module(ds_spirv_sprite_vert, sizeof(ds_spirv_sprite_vert) / sizeof(uint32_t),
                                    &stages[0].module))
        return 0;

    VkVertexInputBindingDescription binding = {0, sizeof(GeoVert), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attrs[3] = {
        {0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(GeoVert, x)},
        {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(GeoVert, u)},
        {2, 0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(GeoVert, c)},
    };
    VkPipelineVertexInputStateCreateInfo vi = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &binding;
    vi.vertexAttributeDescriptionCount = 3;
    vi.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo ia = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = samples;

    VkPipelineDepthStencilStateCreateInfo dss = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};

    VkPipelineColorBlendAttachmentState cb = {0};
    cb.blendEnable = (VkBool32)(blend ? VK_TRUE : VK_FALSE);
    cb.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    cb.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cb.colorBlendOp = VK_BLEND_OP_ADD;
    /* Straight-alpha RGB blend; preserve destination alpha. */
    cb.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    cb.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cb.alphaBlendOp = VK_BLEND_OP_ADD;
    cb.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo cbs = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cbs.attachmentCount = 1;
    cbs.pAttachments = &cb;

    VkDynamicState dyn[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dy = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dy.dynamicStateCount = 2;
    dy.pDynamicStates = dyn;

    VkGraphicsPipelineCreateInfo pi = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pi.stageCount = 2;
    pi.pStages = stages;
    pi.pVertexInputState = &vi;
    pi.pInputAssemblyState = &ia;
    pi.pViewportState = &vp;
    pi.pRasterizationState = &rs;
    pi.pMultisampleState = &ms;
    pi.pDepthStencilState = &dss;
    pi.pColorBlendState = &cbs;
    pi.pDynamicState = &dy;
    pi.layout = layout;
    pi.renderPass = renderpass;
    pi.subpass = 0;
    int ok = ds_vk_check_result(vkCreateGraphicsPipelines(vk_device, VK_NULL_HANDLE, 1, &pi, NULL, out),
                                "vkCreateGraphicsPipelines");

    vkDestroyShaderModule(vk_device, stages[0].module, NULL);
    return ok;
}

static int ds_vk_create_offscreen_image(uint32_t w, uint32_t h, VkFormat format, VkSampleCountFlagBits samples,
                                        VkImageUsageFlags usage, VkImage *image, VkDeviceMemory *memory,
                                        VkImageView *view, const char *label) {
    VkImageCreateInfo ii = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = format;
    ii.extent.width = w;
    ii.extent.height = h;
    ii.extent.depth = 1;
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.samples = samples;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = usage;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (!ds_vk_check_result(vkCreateImage(vk_device, &ii, NULL, image), label))
        return 0;
    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(vk_device, *image, &mr);
    if (!ds_vk_alloc_device_memory(mr, memory))
        return 0;
    if (!ds_vk_check_result(vkBindImageMemory(vk_device, *image, *memory, 0), "vkBindImageMemory(offscreen)"))
        return 0;
    VkImageViewCreateInfo vi = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = *image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = format;
    vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vi.subresourceRange.levelCount = 1;
    vi.subresourceRange.layerCount = 1;
    return ds_vk_check_result(vkCreateImageView(vk_device, &vi, NULL, view), "vkCreateImageView(offscreen)");
}

static void ds_vk_destroy_offscreen_image(VkImage *image, VkDeviceMemory *memory, VkImageView *view) {
    if (!vk_device)
        return;
    if (*view != VK_NULL_HANDLE)
        vkDestroyImageView(vk_device, *view, NULL);
    if (*image != VK_NULL_HANDLE)
        vkDestroyImage(vk_device, *image, NULL);
    if (*memory != VK_NULL_HANDLE)
        vkFreeMemory(vk_device, *memory, NULL);
    *image = VK_NULL_HANDLE;
    *memory = VK_NULL_HANDLE;
    *view = VK_NULL_HANDLE;
}

static int ds_vk_ensure_offscreen(uint32_t w, uint32_t h, VkFormat format) {
    if (vk_off_image != VK_NULL_HANDLE && vk_off_w == w && vk_off_h == h && vk_off_format == format)
        return 1;
    if (vk_device)
        vkDeviceWaitIdle(vk_device);
    ds_vk_destroy_offscreen();

    VkImageUsageFlags resolve_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (ds_vk_smaa_enabled())
        resolve_usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (!ds_vk_create_offscreen_image(w, h, format, VK_SAMPLE_COUNT_1_BIT, resolve_usage, &vk_off_image, &vk_off_mem,
                                      &vk_off_view, "vkCreateImage(offscreen)"))
        goto fail;
    if (vk_msaa_samples != VK_SAMPLE_COUNT_1_BIT &&
        !ds_vk_create_offscreen_image(w, h, format, vk_msaa_samples, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                      &vk_msaa_image, &vk_msaa_mem, &vk_msaa_view, "vkCreateImage(msaa)"))
        goto fail;

    VkImageView scene_attachments[2];
    uint32_t scene_attachment_count = 1;
    scene_attachments[0] = vk_off_view;
    if (vk_msaa_samples != VK_SAMPLE_COUNT_1_BIT) {
        scene_attachments[0] = vk_msaa_view;
        scene_attachments[1] = vk_off_view;
        scene_attachment_count = 2;
    }
    VkFramebufferCreateInfo fi = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fi.renderPass = vk_renderpass;
    fi.attachmentCount = scene_attachment_count;
    fi.pAttachments = scene_attachments;
    fi.width = w;
    fi.height = h;
    fi.layers = 1;
    if (!ds_vk_check_result(vkCreateFramebuffer(vk_device, &fi, NULL, &vk_off_fb), "vkCreateFramebuffer"))
        goto fail;

    if (ds_vk_smaa_enabled()) {
        if (!ds_vk_create_offscreen_image(w, h, format, VK_SAMPLE_COUNT_1_BIT,
                                          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                          &vk_smaa_image, &vk_smaa_mem, &vk_smaa_view, "vkCreateImage(smaa)"))
            goto fail;
        if (!ds_vk_new_descriptor_set(vk_off_view, &vk_smaa_desc))
            goto fail;
        fi.renderPass = vk_smaa_renderpass;
        fi.attachmentCount = 1;
        fi.pAttachments = &vk_smaa_view;
        if (!ds_vk_check_result(vkCreateFramebuffer(vk_device, &fi, NULL, &vk_smaa_fb),
                                "vkCreateFramebuffer(smaa)"))
            goto fail;
    }
    vk_off_w = w;
    vk_off_h = h;
    vk_off_format = format;
    return 1;
fail:
    ds_vk_destroy_offscreen();
    return 0;
}

static void ds_vk_destroy_offscreen(void) {
    if (!vk_device)
        return;
    if (vk_smaa_fb != VK_NULL_HANDLE)
        vkDestroyFramebuffer(vk_device, vk_smaa_fb, NULL);
    vk_smaa_fb = VK_NULL_HANDLE;
    vk_smaa_desc = VK_NULL_HANDLE;
    ds_vk_destroy_offscreen_image(&vk_smaa_image, &vk_smaa_mem, &vk_smaa_view);
    if (vk_off_fb != VK_NULL_HANDLE)
        vkDestroyFramebuffer(vk_device, vk_off_fb, NULL);
    vk_off_fb = VK_NULL_HANDLE;
    ds_vk_destroy_offscreen_image(&vk_msaa_image, &vk_msaa_mem, &vk_msaa_view);
    ds_vk_destroy_offscreen_image(&vk_off_image, &vk_off_mem, &vk_off_view);
    vk_off_w = vk_off_h = 0;
}
