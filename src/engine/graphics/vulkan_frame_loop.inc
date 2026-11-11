static void ds_vk_shutdown_backend(void);

static int ds_vk_init_backend(ANativeWindow *window) {

    if (vk_ready && vk_window == window && vk_surface != VK_NULL_HANDLE)
        return 1;

    if (vk_ready && vk_device != VK_NULL_HANDLE && window) {
        if (vk_surface != VK_NULL_HANDLE)
            ds_vk_surface_lost();
        if (ds_vk_attach_window(window))
            return 1;
        ds_log_err("vulkan: the kept device cannot present to the new window, rebuilding");
    }
    if (vk_ready || vk_device != VK_NULL_HANDLE || vk_instance != VK_NULL_HANDLE)
        ds_vk_shutdown_backend();
    vk_window = window;

    vk_fail_step[0] = '\0';
    vk_fail_code = 0;
    if (!window) {
        ds_log_err("vulkan: no window");
        ds_vk_note_failure("no window", 0);
        return 0;
    }

    VkApplicationInfo app = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "DiScript IDE";
    app.applicationVersion = 1;
    app.pEngineName = "DiScript Vulkan";
    app.engineVersion = 1;
    app.apiVersion = VK_API_VERSION_1_0;
    const char *inst_exts[] = {"VK_KHR_surface", "VK_KHR_android_surface"};
    VkInstanceCreateInfo ii = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ii.pApplicationInfo = &app;
    ii.enabledExtensionCount = 2;
    ii.ppEnabledExtensionNames = inst_exts;
    if (!ds_vk_check_result(vkCreateInstance(&ii, NULL, &vk_instance), "vkCreateInstance"))
        return 0;

    VkAndroidSurfaceCreateInfoKHR sci = {VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR};
    sci.window = window;
    if (!ds_vk_check_result(vkCreateAndroidSurfaceKHR(vk_instance, &sci, NULL, &vk_surface),
                            "vkCreateAndroidSurfaceKHR")) {
        ds_vk_shutdown_backend();
        return 0;
    }

    ds_vk_request_frame_rate(window);

    uint32_t ndev = 0;
    if (!ds_vk_check_result(vkEnumeratePhysicalDevices(vk_instance, &ndev, NULL), "vkEnumeratePhysicalDevices") ||
        ndev == 0) {
        ds_log_err("vulkan: no physical devices");
        ds_vk_note_failure("no Vulkan GPU (vkEnumeratePhysicalDevices = 0)", 0);
        ds_vk_shutdown_backend();
        return 0;
    }
    VkPhysicalDevice *devs = (VkPhysicalDevice *)malloc(ndev * sizeof(*devs));
    if (!devs) {
        ds_vk_shutdown_backend();
        return 0;
    }
    if (!ds_vk_check_result(vkEnumeratePhysicalDevices(vk_instance, &ndev, devs), "vkEnumeratePhysicalDevices")) {
        free(devs);
        ds_vk_shutdown_backend();
        return 0;
    }
    vk_phys = VK_NULL_HANDLE;
    for (uint32_t d = 0; d < ndev && vk_phys == VK_NULL_HANDLE; d++) {
        uint32_t nf = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(devs[d], &nf, NULL);
        if (nf == 0)
            continue;
        VkQueueFamilyProperties *fams = (VkQueueFamilyProperties *)malloc(nf * sizeof(*fams));
        if (!fams)
            break;
        vkGetPhysicalDeviceQueueFamilyProperties(devs[d], &nf, fams);
        for (uint32_t q = 0; q < nf; q++) {
            if (!(fams[q].queueFlags & VK_QUEUE_GRAPHICS_BIT))
                continue;
            VkBool32 present = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(devs[d], q, vk_surface, &present);
            if (present) {
                vk_phys = devs[d];
                vk_queue_family = q;
                break;
            }
        }
        free(fams);
    }
    free(devs);
    if (vk_phys == VK_NULL_HANDLE) {
        ds_log_err("vulkan: no graphics+present queue family");
        ds_vk_note_failure("no queue that can draw and present to the window", 0);
        ds_vk_shutdown_backend();
        return 0;
    }
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(vk_phys, &props);
    snprintf(vk_diag_gpu, sizeof(vk_diag_gpu), "%s", props.deviceName);
    vk_diag_api = props.apiVersion;
    vk_diag_driver = props.driverVersion;
    vk_diag_vendor = props.vendorID;
    ds_log("vulkan: device '%s' (api %u.%u)", props.deviceName, VK_API_VERSION_MAJOR(props.apiVersion),
           VK_API_VERSION_MINOR(props.apiVersion));

    const char *dev_exts[] = {"VK_KHR_swapchain"};
    float prio = 1.0f;
    VkDeviceQueueCreateInfo qi = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qi.queueFamilyIndex = vk_queue_family;
    qi.queueCount = 1;
    qi.pQueuePriorities = &prio;
    VkDeviceCreateInfo di = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    di.queueCreateInfoCount = 1;
    di.pQueueCreateInfos = &qi;
    di.enabledExtensionCount = 1;
    di.ppEnabledExtensionNames = dev_exts;
    if (!ds_vk_check_result(vkCreateDevice(vk_phys, &di, NULL, &vk_device), "vkCreateDevice")) {
        ds_vk_shutdown_backend();
        return 0;
    }
    vkGetDeviceQueue(vk_device, vk_queue_family, 0, &vk_queue);

    VkCommandPoolCreateInfo upi = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    upi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    upi.queueFamilyIndex = vk_queue_family;
    if (!ds_vk_check_result(vkCreateCommandPool(vk_device, &upi, NULL, &vk_upload_pool),
                            "vkCreateCommandPool(upload)")) {
        ds_vk_shutdown_backend();
        return 0;
    }
    VkCommandBufferAllocateInfo uai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    uai.commandPool = vk_upload_pool;
    uai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    uai.commandBufferCount = 1;
    if (!ds_vk_check_result(vkAllocateCommandBuffers(vk_device, &uai, &vk_upload_cmd),
                            "vkAllocateCommandBuffers(upload)")) {
        ds_vk_shutdown_backend();
        return 0;
    }
    VkFenceCreateInfo ufi = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    /* Start unsignaled; each upload submit signals and waits on this fence. */
    ufi.flags = 0;
    if (!ds_vk_check_result(vkCreateFence(vk_device, &ufi, NULL, &vk_upload_fence), "vkCreateFence(upload)")) {
        ds_vk_shutdown_backend();
        return 0;
    }

    VkDescriptorSetLayoutBinding lb = {0};
    lb.binding = 0;
    lb.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    lb.descriptorCount = 1;
    lb.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo dli = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dli.bindingCount = 1;
    dli.pBindings = &lb;
    if (!ds_vk_check_result(vkCreateDescriptorSetLayout(vk_device, &dli, NULL, &vk_desc_layout),
                            "vkCreateDescriptorSetLayout")) {
        ds_vk_shutdown_backend();
        return 0;
    }
    VkSamplerCreateInfo smi = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    smi.magFilter = VK_FILTER_LINEAR;
    smi.minFilter = VK_FILTER_LINEAR;
    smi.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    smi.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    smi.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    smi.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if (!ds_vk_check_result(vkCreateSampler(vk_device, &smi, NULL, &vk_sampler), "vkCreateSampler")) {
        ds_vk_shutdown_backend();
        return 0;
    }

    if (!ds_vk_recreate_swapchain()) {
        ds_vk_shutdown_backend();
        return 0;
    }
    if (!ds_vk_create_renderpass()) {
        ds_vk_shutdown_backend();
        return 0;
    }

    VkPushConstantRange pcr = {VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float) * 4};
    VkPipelineLayoutCreateInfo pli = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pli.pushConstantRangeCount = 1;
    pli.pPushConstantRanges = &pcr;
    if (!ds_vk_check_result(vkCreatePipelineLayout(vk_device, &pli, NULL, &vk_layout_solid),
                            "vkCreatePipelineLayout(solid)")) {
        ds_vk_shutdown_backend();
        return 0;
    }
    pli.setLayoutCount = 1;
    pli.pSetLayouts = &vk_desc_layout;
    if (!ds_vk_check_result(vkCreatePipelineLayout(vk_device, &pli, NULL, &vk_layout_image),
                            "vkCreatePipelineLayout(image)")) {
        ds_vk_shutdown_backend();
        return 0;
    }
    if (!ds_vk_check_result(vkCreatePipelineLayout(vk_device, &pli, NULL, &vk_layout_tint),
                            "vkCreatePipelineLayout(tint)")) {
        ds_vk_shutdown_backend();
        return 0;
    }

    if (!ds_vk_create_all_pipelines()) {
        ds_vk_shutdown_backend();
        return 0;
    }

    if (!ds_vk_create_frame_resources()) {
        ds_vk_shutdown_backend();
        return 0;
    }

    vk_ready = 1;
    ds_log("vulkan: renderer ready");
    return 1;
}

static void ds_vk_shutdown_backend(void) {
    ds_vk_pending_reset();
    ds_vk_release_frame_rate();
    if (vk_device) {
        vkDeviceWaitIdle(vk_device);

        ds_vk_destroy_swapchain();
        ds_vk_destroy_frame_resources();
        ds_vk_destroy_offscreen();
        if (vk_pipe_solid != VK_NULL_HANDLE)
            vkDestroyPipeline(vk_device, vk_pipe_solid, NULL);
        if (vk_pipe_image != VK_NULL_HANDLE)
            vkDestroyPipeline(vk_device, vk_pipe_image, NULL);
        if (vk_pipe_tint != VK_NULL_HANDLE)
            vkDestroyPipeline(vk_device, vk_pipe_tint, NULL);
        if (vk_pipe_replace != VK_NULL_HANDLE)
            vkDestroyPipeline(vk_device, vk_pipe_replace, NULL);
        vk_pipe_solid = vk_pipe_image = vk_pipe_tint = VK_NULL_HANDLE;
        vk_pipe_replace = VK_NULL_HANDLE;
        if (vk_layout_solid != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(vk_device, vk_layout_solid, NULL);
        if (vk_layout_image != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(vk_device, vk_layout_image, NULL);
        if (vk_layout_tint != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(vk_device, vk_layout_tint, NULL);
        vk_layout_solid = vk_layout_image = vk_layout_tint = VK_NULL_HANDLE;
        if (vk_renderpass != VK_NULL_HANDLE)
            vkDestroyRenderPass(vk_device, vk_renderpass, NULL);
        vk_renderpass = VK_NULL_HANDLE;
        if (vk_sampler != VK_NULL_HANDLE)
            vkDestroySampler(vk_device, vk_sampler, NULL);
        vk_sampler = VK_NULL_HANDLE;
        if (vk_desc_layout != VK_NULL_HANDLE)
            vkDestroyDescriptorSetLayout(vk_device, vk_desc_layout, NULL);
        vk_desc_layout = VK_NULL_HANDLE;
        for (size_t i = 0; i < vk_desc_pool_n; i++)
            if (vk_desc_pools[i] != VK_NULL_HANDLE)
                vkDestroyDescriptorPool(vk_device, vk_desc_pools[i], NULL);
        free(vk_desc_pools);
        vk_desc_pools = NULL;
        vk_desc_pool_n = vk_desc_pool_cap = vk_desc_pool_used = 0;
        ds_vk_font_gpu_release();

        for (Texture *t = textures; t; t = t->next) ds_vk_texture_gpu_release(t);
        if (vk_upload_fence != VK_NULL_HANDLE)
            vkDestroyFence(vk_device, vk_upload_fence, NULL);
        if (vk_upload_pool != VK_NULL_HANDLE)
            vkDestroyCommandPool(vk_device, vk_upload_pool, NULL);
        vk_upload_fence = VK_NULL_HANDLE;
        vk_upload_pool = VK_NULL_HANDLE;
        vk_upload_cmd = VK_NULL_HANDLE;
        vkDestroyDevice(vk_device, NULL);
        vk_device = VK_NULL_HANDLE;
    }
    if (vk_surface != VK_NULL_HANDLE && vk_instance != VK_NULL_HANDLE)
        vkDestroySurfaceKHR(vk_instance, vk_surface, NULL);
    vk_surface = VK_NULL_HANDLE;
    if (vk_instance != VK_NULL_HANDLE)
        vkDestroyInstance(vk_instance, NULL);
    vk_instance = VK_NULL_HANDLE;
    vk_phys = VK_NULL_HANDLE;
    vk_queue = VK_NULL_HANDLE;
    vk_window = NULL;
    vk_ready = 0;
    vk_need_recreate = 0;
    vk_image_held = 0;
    vk_begun = 0;
    vk_surface_lost = 0;
    vk_device_lost = 0;
    free(vk_batches);
    vk_batches = NULL;
    vk_batch_n = vk_batch_cap = 0;
}
