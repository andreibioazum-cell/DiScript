#define VK_USE_PLATFORM_ANDROID_KHR
#include <vulkan/vulkan.h>
#include <android/native_window.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <pthread.h>
#include <time.h>

static int g_violations = 0;
static char g_violation_msg[256];
static int g_pipeline_creates = 0;
static int g_swapchain_creates = 0;
static int g_present_suboptimal_once = 1;

static int g_present_suboptimal_always = 0;
static uint64_t g_next_handle = 0x1000;

static int g_instance_creates, g_device_creates, g_surface_creates;
static uint64_t g_live_surfaces[8];
static uint64_t g_swap_surface;
static uint64_t g_swap_handle;
static const void *g_surface_window[8];
static const void *g_dead_window;
static uint64_t g_signalled_sems[16];
static int g_acquires, g_acquire_infinite;
static VkResult g_acquire_next = VK_SUCCESS;
static VkResult g_present_next = VK_SUCCESS;
static int g_surface_live(uint64_t s) {
    for (int i = 0; i < 8; i++)
        if (g_live_surfaces[i] == s && s)
            return i;
    return -1;
}
static int g_sem_signalled(uint64_t s) {
    for (int i = 0; i < 16; i++)
        if (g_signalled_sems[i] == s && s)
            return i;
    return -1;
}
static void g_sem_set(uint64_t s, int on) {
    int i = g_sem_signalled(s);
    if (on && i < 0) {
        for (i = 0; i < 16; i++)
            if (!g_signalled_sems[i]) {
                g_signalled_sems[i] = s;
                return;
            }
    }
    if (!on && i >= 0)
        g_signalled_sems[i] = 0;
}

static VkPresentModeKHR g_present_modes[4] = {VK_PRESENT_MODE_FIFO_KHR, VK_PRESENT_MODE_MAILBOX_KHR,
                                              VK_PRESENT_MODE_FIFO_RELAXED_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR};
static uint32_t g_present_mode_count = 4;
static VkPresentModeKHR g_created_present_mode = VK_PRESENT_MODE_MAX_ENUM_KHR;

static uint32_t g_sw_images = 3;
static int g_present_index_max = -1;
static int g_acquire_last_image = 0;
static VkCompositeAlphaFlagsKHR g_caps_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
static int g_caps_free_extent = 0;
static VkResult g_swapchain_fail_next = VK_SUCCESS;
static VkCompositeAlphaFlagBitsKHR g_created_alpha;
static VkExtent2D g_created_extent;
static uint32_t g_surface_min_images = 2;

static char g_log[16384];
static size_t g_log_len;
static void g_log_append(const char *s) {
    size_t n = strlen(s);
    if (n > sizeof g_log - 2 - g_log_len)
        n = sizeof g_log - 2 - g_log_len;
    memcpy(g_log + g_log_len, s, n);
    g_log_len += n;
    g_log[g_log_len++] = '\n';
    g_log[g_log_len] = '\0';
}

static void g_violation(const char *fmt, ...) {
    if (g_violations)
        return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_violation_msg, sizeof g_violation_msg, fmt, ap);
    va_end(ap);
    g_violations++;
}
static void *g_next(void) {
    return (void *)(g_next_handle++);
}

static int g_validate_pipeline_create(const VkGraphicsPipelineCreateInfo *pi) {
    const char *bad = NULL;
    if (pi->sType != VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO)
        bad = "createInfo.sType";
    else if (pi->pNext)
        bad = "createInfo.pNext != NULL (stack garbage!)";
    else if (pi->flags)
        bad = "createInfo.flags != 0 (stack garbage!)";
    else if (pi->stageCount < 1 || !pi->pStages)
        bad = "createInfo.pStages";
    else if (!pi->layout)
        bad = "createInfo.layout == NULL";
    else if (!pi->renderPass)
        bad = "createInfo.renderPass == NULL";
    else if (pi->subpass != 0)
        bad = "createInfo.subpass != 0";
    for (uint32_t i = 0; i < pi->stageCount && !bad; i++) {
        const VkPipelineShaderStageCreateInfo *st = &pi->pStages[i];
        if (st->sType != VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO)
            bad = "stage.sType";
        else if (st->pNext)
            bad = "stage.pNext != NULL (stack garbage!)";
        else if (st->flags)
            bad = "stage.flags != 0 (stack garbage!)";
        else if (st->module == VK_NULL_HANDLE)
            bad = "stage.module == NULL";
        else if (!st->pName)
            bad = "stage.pName == NULL";
    }
    struct {
        const void *p;
        const char *name;
        VkStructureType type;
    } subs[8];
    int n = 0;
    if (pi->pVertexInputState) {
        subs[n].p = pi->pVertexInputState;
        subs[n].name = "vertexInput";
        subs[n].type = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        n++;
    }
    if (pi->pInputAssemblyState) {
        subs[n].p = pi->pInputAssemblyState;
        subs[n].name = "inputAssembly";
        subs[n].type = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        n++;
    }
    if (pi->pViewportState) {
        subs[n].p = pi->pViewportState;
        subs[n].name = "viewport";
        subs[n].type = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        n++;
    }
    if (pi->pRasterizationState) {
        subs[n].p = pi->pRasterizationState;
        subs[n].name = "rasterization";
        subs[n].type = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        n++;
    }
    if (pi->pMultisampleState) {
        subs[n].p = pi->pMultisampleState;
        subs[n].name = "multisample";
        subs[n].type = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        n++;
    }
    if (pi->pDepthStencilState) {
        subs[n].p = pi->pDepthStencilState;
        subs[n].name = "depthStencil";
        subs[n].type = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        n++;
    }
    if (pi->pColorBlendState) {
        subs[n].p = pi->pColorBlendState;
        subs[n].name = "colorBlend";
        subs[n].type = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        n++;
    }
    if (pi->pDynamicState) {
        subs[n].p = pi->pDynamicState;
        subs[n].name = "dynamicState";
        subs[n].type = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        n++;
    }
    for (int i = 0; i < n && !bad; i++) {
        const VkBaseInStructure *b = (const VkBaseInStructure *)subs[i].p;
        if (b->sType != subs[i].type)
            bad = subs[i].name;
        else if (b->pNext)
            bad = subs[i].name;
    }
    if (bad) {
        g_violation("vkCreateGraphicsPipelines: %s", bad);
        return 0;
    }
    return 1;
}

static void g_fill_format_props(VkFormatProperties *fp) {
    memset(fp, 0, sizeof *fp);
    fp->optimalTilingFeatures = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_BLIT_SRC_BIT |
                                VK_FORMAT_FEATURE_BLIT_DST_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
                                VK_FORMAT_FEATURE_TRANSFER_DST_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
}

VkResult vkCreateInstance(const VkInstanceCreateInfo *ci, const VkAllocationCallbacks *ac, VkInstance *out) {
    (void)ac;
    if (ci->pNext) {
        g_violation("vkCreateInstance: pNext != NULL");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    *out = (VkInstance)g_next();
    g_instance_creates++;
    return VK_SUCCESS;
}
void vkDestroyInstance(VkInstance i, const VkAllocationCallbacks *ac) {
    (void)i;
    (void)ac;
}
VkResult vkEnumeratePhysicalDevices(VkInstance i, uint32_t *count, VkPhysicalDevice *devs) {
    (void)i;
    static VkPhysicalDevice dev = (VkPhysicalDevice)0x5001;
    if (!devs) {
        *count = 1;
        return VK_SUCCESS;
    }
    devs[0] = dev;
    *count = 1;
    return VK_SUCCESS;
}
void vkGetPhysicalDeviceProperties(VkPhysicalDevice d, VkPhysicalDeviceProperties *p) {
    (void)d;
    memset(p, 0, sizeof *p);
    p->apiVersion = VK_API_VERSION_1_1;
    p->deviceType = VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
    /* Exercise the actual MSAA allocation/pipeline path, not only the
     * single-sample fallback used by a feature-poor fake GPU. */
    p->limits.framebufferColorSampleCounts = VK_SAMPLE_COUNT_4_BIT;
    strcpy(p->deviceName, "FakeMali-G57");
}
VkResult vkGetPhysicalDeviceImageFormatProperties(VkPhysicalDevice d, VkFormat format, VkImageType type,
                                                  VkImageTiling tiling, VkImageUsageFlags usage,
                                                  VkImageCreateFlags flags, VkImageFormatProperties *p) {
    (void)d;
    (void)format;
    (void)type;
    (void)tiling;
    (void)usage;
    (void)flags;
    memset(p, 0, sizeof *p);
    p->sampleCounts = VK_SAMPLE_COUNT_4_BIT;
    return VK_SUCCESS;
}
void vkGetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice d, uint32_t *count, VkQueueFamilyProperties *fams) {
    (void)d;
    if (!fams) {
        *count = 1;
        return;
    }
    memset(fams, 0, sizeof *fams);
    fams->queueFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;
    fams->timestampValidBits = 64;
    fams->minImageTransferGranularity.width = 1;
    fams->minImageTransferGranularity.height = 1;
    *count = 1;
}
VkResult vkCreateDevice(VkPhysicalDevice d, const VkDeviceCreateInfo *ci, const VkAllocationCallbacks *ac,
                        VkDevice *out) {
    (void)d;
    (void)ac;
    if (ci->pNext) {
        g_violation("vkCreateDevice: pNext != NULL");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    *out = (VkDevice)g_next();
    g_device_creates++;
    return VK_SUCCESS;
}
void vkGetDeviceQueue(VkDevice d, uint32_t fam, uint32_t idx, VkQueue *q) {
    (void)d;
    (void)idx;
    *q = (VkQueue)(uintptr_t)(0x6000 + fam);
}
void vkDestroyDevice(VkDevice d, const VkAllocationCallbacks *ac) {
    (void)d;
    (void)ac;
}
VkResult vkDeviceWaitIdle(VkDevice d) {
    (void)d;
    return VK_SUCCESS;
}
VkResult vkQueueWaitIdle(VkQueue q) {
    (void)q;
    return VK_SUCCESS;
}
VkResult vkQueueSubmit(VkQueue q, uint32_t n, const VkSubmitInfo *si, VkFence f) {
    (void)q;
    (void)f;
    for (uint32_t i = 0; i < n; i++)
        for (uint32_t w = 0; w < si[i].waitSemaphoreCount; w++)
            g_sem_set((uint64_t)(uintptr_t)si[i].pWaitSemaphores[w], 0);
    return VK_SUCCESS;
}
VkResult vkQueuePresentKHR(VkQueue q, const VkPresentInfoKHR *pi) {
    (void)q;
    if (pi && pi->swapchainCount && (uint64_t)(uintptr_t)pi->pSwapchains[0] != g_swap_handle)
        g_violation("present: swapchain is not the live one");
    int si = g_surface_live(g_swap_surface);
    if (si >= 0 && g_dead_window && g_surface_window[si] == g_dead_window)
        g_violation("present: the swapchain belongs to a window Android already took away");
    if (pi && pi->swapchainCount && pi->pImageIndices) {
        if (pi->pImageIndices[0] >= g_sw_images)
            g_violation("present: image index %u of %u", pi->pImageIndices[0], g_sw_images);
        if ((int)pi->pImageIndices[0] > g_present_index_max)
            g_present_index_max = (int)pi->pImageIndices[0];
    }
    if (g_present_next != VK_SUCCESS) {
        VkResult r = g_present_next;
        g_present_next = VK_SUCCESS;
        return r;
    }
    if (g_present_suboptimal_always)
        return VK_SUBOPTIMAL_KHR;
    if (g_present_suboptimal_once) {
        g_present_suboptimal_once = 0;
        return VK_SUBOPTIMAL_KHR;
    }
    return VK_SUCCESS;
}
VkResult vkGetPhysicalDeviceSurfacePresentModesKHR(VkPhysicalDevice d, VkSurfaceKHR s, uint32_t *count,
                                                   VkPresentModeKHR *modes) {
    (void)d;
    (void)s;
    if (!modes) {
        *count = g_present_mode_count;
        return VK_SUCCESS;
    }
    uint32_t n = *count < g_present_mode_count ? *count : g_present_mode_count;
    for (uint32_t i = 0; i < n; i++) modes[i] = g_present_modes[i];
    *count = g_present_mode_count;
    return VK_SUCCESS;
}
VkResult vkCreateSwapchainKHR(VkDevice d, const VkSwapchainCreateInfoKHR *ci, const VkAllocationCallbacks *ac,
                              VkSwapchainKHR *out) {
    (void)d;
    (void)ac;
    if (g_swapchain_fail_next != VK_SUCCESS) {
        VkResult r = g_swapchain_fail_next;
        g_swapchain_fail_next = VK_SUCCESS;
        return r;
    }
    if (ci->imageExtent.width == 0 || ci->imageExtent.height == 0) {
        g_violation("swapchain: empty extent");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (!(ci->compositeAlpha & g_caps_alpha)) {
        g_violation("swapchain: compositeAlpha %d is not listed by the surface (0x%x)", (int)ci->compositeAlpha,
                    (unsigned)g_caps_alpha);
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    g_created_alpha = ci->compositeAlpha;
    g_created_extent = ci->imageExtent;

    if (ci->preTransform != VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
        g_violation("swapchain: preTransform != IDENTITY (the landscape picture would be rotated)");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    int listed = 0;
    for (uint32_t i = 0; i < g_present_mode_count; i++)
        if (g_present_modes[i] == ci->presentMode)
            listed = 1;
    if (!listed) {
        g_violation("swapchain: presentMode %d is not in the driver list", (int)ci->presentMode);
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (g_surface_live((uint64_t)(uintptr_t)ci->surface) < 0) {
        g_violation("swapchain: created on a surface that does not exist");
        return VK_ERROR_SURFACE_LOST_KHR;
    }
    if (g_swap_handle)
        g_violation("swapchain: a second swapchain while the old one is alive");
    g_created_present_mode = ci->presentMode;
    g_swapchain_creates++;
    *out = (VkSwapchainKHR)g_next();
    g_swap_handle = (uint64_t)(uintptr_t)*out;
    g_swap_surface = (uint64_t)(uintptr_t)ci->surface;
    return VK_SUCCESS;
}
void vkDestroySwapchainKHR(VkDevice d, VkSwapchainKHR s, const VkAllocationCallbacks *ac) {
    (void)d;
    (void)ac;
    if ((uint64_t)(uintptr_t)s == g_swap_handle) {
        g_swap_handle = 0;
        g_swap_surface = 0;
    }
}

VkResult vkGetSwapchainImagesKHR(VkDevice d, VkSwapchainKHR s, uint32_t *count, VkImage *imgs) {
    (void)d;
    (void)s;
    if (!imgs) {
        *count = g_sw_images;
        return VK_SUCCESS;
    }
    uint32_t n = *count < g_sw_images ? *count : g_sw_images;
    for (uint32_t i = 0; i < n; i++) imgs[i] = (VkImage)g_next();
    *count = n;
    return n < g_sw_images ? VK_INCOMPLETE : VK_SUCCESS;
}
VkResult vkAcquireNextImageKHR(VkDevice d, VkSwapchainKHR s, uint64_t t, VkSemaphore sem, VkFence f, uint32_t *idx) {
    (void)d;
    (void)f;
    g_acquires++;
    if (t == UINT64_MAX)
        g_acquire_infinite++;
    if ((uint64_t)(uintptr_t)s != g_swap_handle)
        g_violation("acquire: swapchain is not the live one");
    int si = g_surface_live(g_swap_surface);
    if (si >= 0 && g_dead_window && g_surface_window[si] == g_dead_window)
        g_violation("acquire: the swapchain belongs to a window Android already took away");
    if (g_acquire_next != VK_SUCCESS) {
        VkResult r = g_acquire_next;
        g_acquire_next = VK_SUCCESS;
        return r;
    }

    if (g_sem_signalled((uint64_t)(uintptr_t)sem) >= 0)
        g_violation("acquire: the semaphore still has a pending signal from an earlier acquire");
    g_sem_set((uint64_t)(uintptr_t)sem, 1);
    *idx = g_acquire_last_image ? g_sw_images - 1 : 0;
    return VK_SUCCESS;
}
VkResult vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VkPhysicalDevice d, VkSurfaceKHR s, VkSurfaceCapabilitiesKHR *c) {
    (void)d;
    (void)s;
    memset(c, 0, sizeof *c);
    c->currentExtent.width = 720;
    c->currentExtent.height = 1280;
    if (g_caps_free_extent) {
        c->currentExtent.width = UINT32_MAX;
        c->currentExtent.height = UINT32_MAX;
    }
    c->minImageExtent.width = 1;
    c->minImageExtent.height = 1;
    c->maxImageExtent.width = 4096;
    c->maxImageExtent.height = 4096;
    c->supportedUsageFlags =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    c->minImageCount = g_surface_min_images;
    c->maxImageCount = 0;
    c->maxImageArrayLayers = 1;

    c->supportedTransforms = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR | VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR;
    c->currentTransform = VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR;
    c->supportedCompositeAlpha = g_caps_alpha;
    return VK_SUCCESS;
}
VkResult vkGetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice d, VkSurfaceKHR s, uint32_t *count,
                                              VkSurfaceFormatKHR *fmts) {
    (void)d;
    (void)s;

    VkFormat f = (g_swapchain_creates == 0) ? VK_FORMAT_B8G8R8A8_UNORM : VK_FORMAT_R8G8B8A8_UNORM;
    if (!fmts) {
        *count = 1;
        return VK_SUCCESS;
    }
    fmts[0].format = f;
    fmts[0].colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    *count = 1;
    return VK_SUCCESS;
}
VkResult vkGetPhysicalDeviceSurfaceSupportKHR(VkPhysicalDevice d, uint32_t q, VkSurfaceKHR s, VkBool32 *ok) {
    (void)d;
    (void)q;
    (void)s;
    *ok = VK_TRUE;
    return VK_SUCCESS;
}
void vkGetPhysicalDeviceFormatProperties(VkPhysicalDevice d, VkFormat f, VkFormatProperties *fp) {
    (void)d;
    (void)f;
    g_fill_format_props(fp);
}
VkResult vkCreateAndroidSurfaceKHR(VkInstance i, const VkAndroidSurfaceCreateInfoKHR *ci,
                                   const VkAllocationCallbacks *ac, VkSurfaceKHR *out) {
    (void)i;
    (void)ac;
    if (!ci->window) {
        g_violation("android surface: window == NULL");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    if (ci->window == g_dead_window) {
        g_violation("android surface: created on a window that is gone");
        return VK_ERROR_NATIVE_WINDOW_IN_USE_KHR;
    }
    *out = (VkSurfaceKHR)g_next();
    g_surface_creates++;
    for (int k = 0; k < 8; k++)
        if (!g_live_surfaces[k]) {
            g_live_surfaces[k] = (uint64_t)(uintptr_t)*out;
            g_surface_window[k] = ci->window;
            break;
        }
    return VK_SUCCESS;
}
void vkDestroySurfaceKHR(VkInstance i, VkSurfaceKHR s, const VkAllocationCallbacks *ac) {
    (void)i;
    (void)ac;
    int k = g_surface_live((uint64_t)(uintptr_t)s);
    if (k < 0) {
        g_violation("vkDestroySurfaceKHR: unknown or already destroyed surface");
        return;
    }
    if (g_swap_handle && g_swap_surface == (uint64_t)(uintptr_t)s)
        g_violation("vkDestroySurfaceKHR: the surface still has a live swapchain");
    g_live_surfaces[k] = 0;
    g_surface_window[k] = NULL;
}
void vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice d, VkPhysicalDeviceMemoryProperties *mp) {
    (void)d;
    memset(mp, 0, sizeof *mp);
    mp->memoryTypeCount = 2;
    mp->memoryTypes[0].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    mp->memoryTypes[1].propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    mp->memoryHeapCount = 1;
    mp->memoryHeaps[0].size = 0x10000000;
    mp->memoryHeaps[0].flags = VK_MEMORY_HEAP_DEVICE_LOCAL_BIT;
}
VkResult vkAllocateMemory(VkDevice d, const VkMemoryAllocateInfo *ai, const VkAllocationCallbacks *ac,
                          VkDeviceMemory *out) {
    (void)d;
    (void)ai;
    (void)ac;
    *out = (VkDeviceMemory)g_next();
    return VK_SUCCESS;
}
void vkFreeMemory(VkDevice d, VkDeviceMemory m, const VkAllocationCallbacks *ac) {
    (void)d;
    (void)m;
    (void)ac;
}
VkResult vkMapMemory(VkDevice d, VkDeviceMemory m, VkDeviceSize off, VkDeviceSize size, VkMemoryMapFlags fl,
                     void **pp) {
    (void)d;
    (void)m;
    (void)off;
    (void)fl;
    *pp = malloc(size ? size : 1);
    return *pp ? VK_SUCCESS : VK_ERROR_OUT_OF_HOST_MEMORY;
}
void vkUnmapMemory(VkDevice d, VkDeviceMemory m) {
    (void)d;
    (void)m;
}
VkResult vkCreateBuffer(VkDevice d, const VkBufferCreateInfo *ci, const VkAllocationCallbacks *ac, VkBuffer *out) {
    (void)d;
    (void)ci;
    (void)ac;
    *out = (VkBuffer)g_next();
    return VK_SUCCESS;
}
void vkDestroyBuffer(VkDevice d, VkBuffer b, const VkAllocationCallbacks *ac) {
    (void)d;
    (void)b;
    (void)ac;
}
void vkGetBufferMemoryRequirements(VkDevice d, VkBuffer b, VkMemoryRequirements *mr) {
    (void)d;
    (void)b;
    memset(mr, 0, sizeof *mr);
    mr->size = 4096;
    mr->alignment = 256;
    mr->memoryTypeBits = 0x3;
}
VkResult vkBindBufferMemory(VkDevice d, VkBuffer b, VkDeviceMemory m, VkDeviceSize off) {
    (void)d;
    (void)b;
    (void)m;
    (void)off;
    return VK_SUCCESS;
}
VkResult vkCreateImage(VkDevice d, const VkImageCreateInfo *ci, const VkAllocationCallbacks *ac, VkImage *out) {
    (void)d;
    (void)ci;
    (void)ac;
    *out = (VkImage)g_next();
    return VK_SUCCESS;
}
void vkDestroyImage(VkDevice d, VkImage i, const VkAllocationCallbacks *ac) {
    (void)d;
    (void)i;
    (void)ac;
}
void vkGetImageMemoryRequirements(VkDevice d, VkImage i, VkMemoryRequirements *mr) {
    (void)d;
    (void)i;
    memset(mr, 0, sizeof *mr);
    mr->size = 4096;
    mr->alignment = 256;
    mr->memoryTypeBits = 0x1;
}
VkResult vkBindImageMemory(VkDevice d, VkImage i, VkDeviceMemory m, VkDeviceSize off) {
    (void)d;
    (void)i;
    (void)m;
    (void)off;
    return VK_SUCCESS;
}
VkResult vkCreateImageView(VkDevice d, const VkImageViewCreateInfo *ci, const VkAllocationCallbacks *ac,
                           VkImageView *out) {
    (void)d;
    (void)ci;
    (void)ac;
    *out = (VkImageView)g_next();
    return VK_SUCCESS;
}
void vkDestroyImageView(VkDevice d, VkImageView v, const VkAllocationCallbacks *ac) {
    (void)d;
    (void)v;
    (void)ac;
}
VkResult vkCreateRenderPass(VkDevice d, const VkRenderPassCreateInfo *ci, const VkAllocationCallbacks *ac,
                            VkRenderPass *out) {
    (void)d;
    (void)ac;
    if (ci->pNext) {
        g_violation("vkCreateRenderPass: pNext != NULL");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    *out = (VkRenderPass)g_next();
    return VK_SUCCESS;
}
void vkDestroyRenderPass(VkDevice d, VkRenderPass rp, const VkAllocationCallbacks *ac) {
    (void)d;
    (void)rp;
    (void)ac;
}
VkResult vkCreateFramebuffer(VkDevice d, const VkFramebufferCreateInfo *ci, const VkAllocationCallbacks *ac,
                             VkFramebuffer *out) {
    (void)d;
    (void)ci;
    (void)ac;
    *out = (VkFramebuffer)g_next();
    return VK_SUCCESS;
}
void vkDestroyFramebuffer(VkDevice d, VkFramebuffer fb, const VkAllocationCallbacks *ac) {
    (void)d;
    (void)fb;
    (void)ac;
}
VkResult vkCreatePipelineLayout(VkDevice d, const VkPipelineLayoutCreateInfo *ci, const VkAllocationCallbacks *ac,
                                VkPipelineLayout *out) {
    (void)d;
    (void)ci;
    (void)ac;
    *out = (VkPipelineLayout)g_next();
    return VK_SUCCESS;
}
void vkDestroyPipelineLayout(VkDevice d, VkPipelineLayout pl, const VkAllocationCallbacks *ac) {
    (void)d;
    (void)pl;
    (void)ac;
}
VkResult vkCreateShaderModule(VkDevice d, const VkShaderModuleCreateInfo *ci, const VkAllocationCallbacks *ac,
                              VkShaderModule *out) {
    (void)d;
    (void)ac;
    if (ci->pNext) {
        g_violation("vkCreateShaderModule: pNext != NULL");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    *out = (VkShaderModule)g_next();
    return VK_SUCCESS;
}
void vkDestroyShaderModule(VkDevice d, VkShaderModule sm, const VkAllocationCallbacks *ac) {
    (void)d;
    (void)sm;
    (void)ac;
}
VkResult vkCreateGraphicsPipelines(VkDevice d, VkPipelineCache pc, uint32_t n, const VkGraphicsPipelineCreateInfo *cis,
                                   const VkAllocationCallbacks *ac, VkPipeline *out) {
    (void)d;
    (void)pc;
    (void)n;
    (void)ac;
    g_pipeline_creates++;
    if (!g_validate_pipeline_create(cis))
        return VK_ERROR_INITIALIZATION_FAILED;
    *out = (VkPipeline)g_next();
    return VK_SUCCESS;
}
void vkDestroyPipeline(VkDevice d, VkPipeline p, const VkAllocationCallbacks *ac) {
    (void)d;
    (void)p;
    (void)ac;
}
VkResult vkCreateSampler(VkDevice d, const VkSamplerCreateInfo *ci, const VkAllocationCallbacks *ac, VkSampler *out) {
    (void)d;
    (void)ci;
    (void)ac;
    *out = (VkSampler)g_next();
    return VK_SUCCESS;
}
void vkDestroySampler(VkDevice d, VkSampler s, const VkAllocationCallbacks *ac) {
    (void)d;
    (void)s;
    (void)ac;
}
VkResult vkCreateDescriptorSetLayout(VkDevice d, const VkDescriptorSetLayoutCreateInfo *ci,
                                     const VkAllocationCallbacks *ac, VkDescriptorSetLayout *out) {
    (void)d;
    (void)ci;
    (void)ac;
    *out = (VkDescriptorSetLayout)g_next();
    return VK_SUCCESS;
}
void vkDestroyDescriptorSetLayout(VkDevice d, VkDescriptorSetLayout l, const VkAllocationCallbacks *ac) {
    (void)d;
    (void)l;
    (void)ac;
}
VkResult vkCreateDescriptorPool(VkDevice d, const VkDescriptorPoolCreateInfo *ci, const VkAllocationCallbacks *ac,
                                VkDescriptorPool *out) {
    (void)d;
    (void)ci;
    (void)ac;
    *out = (VkDescriptorPool)g_next();
    return VK_SUCCESS;
}
void vkDestroyDescriptorPool(VkDevice d, VkDescriptorPool p, const VkAllocationCallbacks *ac) {
    (void)d;
    (void)p;
    (void)ac;
}

static VkDescriptorPool g_desc_refuse_pool = VK_NULL_HANDLE;
static int g_desc_refusals;
VkResult vkAllocateDescriptorSets(VkDevice d, const VkDescriptorSetAllocateInfo *ai, VkDescriptorSet *sets) {
    (void)d;
    if (g_desc_refuse_pool != VK_NULL_HANDLE && ai->descriptorPool == g_desc_refuse_pool) {
        g_desc_refusals++;
        return VK_ERROR_OUT_OF_POOL_MEMORY;
    }
    *sets = (VkDescriptorSet)g_next();
    return VK_SUCCESS;
}
void vkUpdateDescriptorSets(VkDevice d, uint32_t nw, const VkWriteDescriptorSet *w, uint32_t nc,
                            const VkCopyDescriptorSet *c) {
    (void)d;
    (void)nw;
    (void)w;
    (void)nc;
    (void)c;
}
VkResult vkCreateCommandPool(VkDevice d, const VkCommandPoolCreateInfo *ci, const VkAllocationCallbacks *ac,
                             VkCommandPool *out) {
    (void)d;
    (void)ci;
    (void)ac;
    *out = (VkCommandPool)g_next();
    return VK_SUCCESS;
}
void vkDestroyCommandPool(VkDevice d, VkCommandPool p, const VkAllocationCallbacks *ac) {
    (void)d;
    (void)p;
    (void)ac;
}
VkResult vkAllocateCommandBuffers(VkDevice d, const VkCommandBufferAllocateInfo *ai, VkCommandBuffer *cbs) {
    (void)d;
    (void)ai;
    *cbs = (VkCommandBuffer)g_next();
    return VK_SUCCESS;
}
VkResult vkResetCommandBuffer(VkCommandBuffer cb, VkCommandBufferResetFlags f) {
    (void)cb;
    (void)f;
    return VK_SUCCESS;
}
VkResult vkBeginCommandBuffer(VkCommandBuffer cb, const VkCommandBufferBeginInfo *bi) {
    (void)cb;
    (void)bi;
    return VK_SUCCESS;
}
VkResult vkEndCommandBuffer(VkCommandBuffer cb) {
    (void)cb;
    return VK_SUCCESS;
}
VkResult vkCreateFence(VkDevice d, const VkFenceCreateInfo *ci, const VkAllocationCallbacks *ac, VkFence *out) {
    (void)d;
    (void)ci;
    (void)ac;
    *out = (VkFence)g_next();
    return VK_SUCCESS;
}
void vkDestroyFence(VkDevice d, VkFence f, const VkAllocationCallbacks *ac) {
    (void)d;
    (void)f;
    (void)ac;
}
VkResult vkResetFences(VkDevice d, uint32_t n, const VkFence *fs) {
    (void)d;
    (void)n;
    (void)fs;
    return VK_SUCCESS;
}
VkResult vkWaitForFences(VkDevice d, uint32_t n, const VkFence *fs, VkBool32 all, uint64_t t) {
    (void)d;
    (void)n;
    (void)fs;
    (void)all;
    (void)t;
    return VK_SUCCESS;
}
VkResult vkCreateSemaphore(VkDevice d, const VkSemaphoreCreateInfo *ci, const VkAllocationCallbacks *ac,
                           VkSemaphore *out) {
    (void)d;
    (void)ci;
    (void)ac;
    *out = (VkSemaphore)g_next();
    return VK_SUCCESS;
}
void vkDestroySemaphore(VkDevice d, VkSemaphore s, const VkAllocationCallbacks *ac) {
    (void)d;
    (void)ac;
    g_sem_set((uint64_t)(uintptr_t)s, 0);
}

static int g_frame_rate_calls, g_frame_rate_strategy_calls, g_clear_frame_rate_calls;
static ANativeWindow *g_frame_rate_window;
static ANativeWindow *g_test_window;
static float g_frame_rate_value;
static int8_t g_frame_rate_compat, g_frame_rate_strategy;
static int32_t fake_set_frame_rate(ANativeWindow *w, float fps, int8_t compat) {
    g_frame_rate_calls++;
    g_frame_rate_window = w;
    g_frame_rate_value = fps;
    g_frame_rate_compat = compat;
    return 0;
}
static int32_t fake_set_frame_rate_strategy(ANativeWindow *w, float fps, int8_t compat, int8_t strategy) {
    g_frame_rate_strategy_calls++;
    g_frame_rate_window = w;
    g_frame_rate_value = fps;
    g_frame_rate_compat = compat;
    g_frame_rate_strategy = strategy;
    return 0;
}
static void fake_clear_frame_rate(ANativeWindow *w) {
    (void)w;
    g_clear_frame_rate_calls++;
}

void vkCmdBindPipeline(VkCommandBuffer cb, VkPipelineBindPoint pb, VkPipeline p) {
    (void)cb;
    (void)pb;
    (void)p;
}

static float g_pc[4];
static int g_pc_sets, g_blits, g_copies;
static int g_blit_src_w, g_blit_src_h, g_blit_dst_w, g_blit_dst_h;
void vkCmdPushConstants(VkCommandBuffer cb, VkPipelineLayout l, VkShaderStageFlags s, uint32_t off, uint32_t size,
                        const void *v) {
    (void)cb;
    (void)l;
    (void)s;
    (void)off;
    if (size >= sizeof g_pc && v)
        memcpy(g_pc, v, sizeof g_pc);
    g_pc_sets++;
}
void vkCmdBindDescriptorSets(VkCommandBuffer cb, VkPipelineBindPoint pb, VkPipelineLayout l, uint32_t first, uint32_t n,
                             const VkDescriptorSet *sets, uint32_t din, const uint32_t *dyn) {
    (void)cb;
    (void)pb;
    (void)l;
    (void)first;
    (void)n;
    (void)sets;
    (void)din;
    (void)dyn;
}
void vkCmdBindVertexBuffers(VkCommandBuffer cb, uint32_t first, uint32_t n, const VkBuffer *bufs,
                            const VkDeviceSize *offs) {
    (void)cb;
    (void)first;
    (void)n;
    (void)bufs;
    (void)offs;
}
void vkCmdBindIndexBuffer(VkCommandBuffer cb, VkBuffer b, VkDeviceSize off, VkIndexType t) {
    (void)cb;
    (void)b;
    (void)off;
    (void)t;
}
void vkCmdDrawIndexed(VkCommandBuffer cb, uint32_t idx, uint32_t draws, uint32_t first, int32_t off, uint32_t firsti) {
    (void)cb;
    (void)idx;
    (void)draws;
    (void)first;
    (void)off;
    (void)firsti;
}
void vkCmdBeginRenderPass(VkCommandBuffer cb, const VkRenderPassBeginInfo *bi, VkSubpassContents c) {
    (void)cb;
    (void)bi;
    (void)c;
}
void vkCmdEndRenderPass(VkCommandBuffer cb) {
    (void)cb;
}
void vkCmdSetViewport(VkCommandBuffer cb, uint32_t first, uint32_t n, const VkViewport *vps) {
    (void)cb;
    (void)first;
    (void)n;
    (void)vps;
}
void vkCmdSetScissor(VkCommandBuffer cb, uint32_t first, uint32_t n, const VkRect2D *rc) {
    (void)cb;
    (void)first;
    (void)n;
    (void)rc;
}
void vkCmdPipelineBarrier(VkCommandBuffer cb, VkPipelineStageFlags s, VkPipelineStageFlags d, VkDependencyFlags fl,
                          uint32_t nm, const VkMemoryBarrier *mb, uint32_t nb, const VkBufferMemoryBarrier *bb,
                          uint32_t ni, const VkImageMemoryBarrier *ib) {
    (void)cb;
    (void)s;
    (void)d;
    (void)fl;
    (void)nm;
    (void)mb;
    (void)nb;
    (void)bb;
    (void)ni;
    (void)ib;
}
void vkCmdCopyBufferToImage(VkCommandBuffer cb, VkBuffer src, VkImage dst, VkImageLayout dl, uint32_t n,
                            const VkBufferImageCopy *rc) {
    (void)cb;
    (void)src;
    (void)dst;
    (void)dl;
    (void)n;
    (void)rc;
}
void vkCmdBlitImage(VkCommandBuffer cb, VkImage src, VkImageLayout sl, VkImage dst, VkImageLayout dl, uint32_t n,
                    const VkImageBlit *rc, VkFilter f) {
    (void)cb;
    (void)src;
    (void)sl;
    (void)dst;
    (void)dl;
    if (n == 1 && rc) {
        g_blit_src_w = rc[0].srcOffsets[1].x;
        g_blit_src_h = rc[0].srcOffsets[1].y;
        g_blit_dst_w = rc[0].dstOffsets[1].x;
        g_blit_dst_h = rc[0].dstOffsets[1].y;
        int same_size =
            rc[0].srcOffsets[1].x == rc[0].dstOffsets[1].x && rc[0].srcOffsets[1].y == rc[0].dstOffsets[1].y;

        if (same_size && f != VK_FILTER_NEAREST)
            g_violation("blit 1:1: filter is not NEAREST");
        if (!same_size && f != VK_FILTER_LINEAR)
            g_violation("autoscale blit: filter is not LINEAR (the stretch must be soft)");
    }
    g_blits++;
}
void vkCmdCopyImage(VkCommandBuffer cb, VkImage src, VkImageLayout sl, VkImage dst, VkImageLayout dl, uint32_t n,
                    const VkImageCopy *rc) {
    (void)cb;
    (void)src;
    (void)sl;
    (void)dst;
    (void)dl;
    (void)n;
    (void)rc;
    g_copies++;
}

static void g_log_vprintf(const char *format, va_list ap) {
    char line[256];
    vsnprintf(line, sizeof line, format, ap);
    g_log_append(line);
}
void ds_log(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    g_log_vprintf(format, ap);
    va_end(ap);
}
void ds_log_err(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    g_log_vprintf(format, ap);
    va_end(ap);
}
void ds_console_log(int is_error, const char *format, ...) {
    (void)is_error;
    (void)format;
}
void ds_runtime_error(const char *format, ...) {
    (void)format;
}
const char *ds_runtime_error_message(void) {
    return "";
}
int console_count(void) {
    return 0;
}
const char *console_line(int i) {
    (void)i;
    return "";
}
int console_type(int i) {
    (void)i;
    return 0;
}
int screen_w = 720, screen_h = 1280;
double dt = 0.0;

#include "engine/graphics.c"

static int g_real_assets = 0;
struct AAsset {
    FILE *f;
    off_t len;
};
AAsset *AAssetManager_open(AAssetManager *mgr, const char *name, int mode) {
    (void)mgr;
    (void)mode;
    if (!g_real_assets || !name)
        return NULL;
    char path[512];
    snprintf(path, sizeof path, "assets/textures/%s", name);
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    AAsset *a = (AAsset *)calloc(1, sizeof *a);
    if (!a) {
        fclose(f);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    a->len = (off_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    a->f = f;
    return a;
}
off_t AAsset_getLength(AAsset *a) {
    return a ? a->len : 0;
}
int AAsset_read(AAsset *a, void *buf, size_t n) {
    return a ? (int)fread(buf, 1, n, a->f) : -1;
}
int AAsset_close(AAsset *a) {
    if (a) {
        fclose(a->f);
        free(a);
    }
    return 0;
}
/* Asset directory enumeration is not used by this Vulkan harness, but the
 * graphics translation unit references its Android declarations. */
struct AAssetDir {};
AAssetDir *AAssetManager_openDir(AAssetManager *mgr, const char *dirName) {
    (void)mgr;
    (void)dirName;
    return NULL;
}
const char *AAssetDir_getNextFileName(AAssetDir *assetDir) {
    (void)assetDir;
    return NULL;
}
void AAssetDir_close(AAssetDir *assetDir) { (void)assetDir; }

static int32_t g_win_w = 720, g_win_h = 1280;
static uint32_t *g_win_pixels;
static int g_win_locks, g_win_posts, g_win_lock_refused;
int32_t ANativeWindow_getWidth(ANativeWindow *w) {
    (void)w;
    return g_win_w;
}
int32_t ANativeWindow_getHeight(ANativeWindow *w) {
    (void)w;
    return g_win_h;
}
int32_t ANativeWindow_setBuffersGeometry(ANativeWindow *w, int32_t width, int32_t height, int32_t format) {
    (void)w;
    (void)width;
    (void)height;
    (void)format;
    return 0;
}
int ANativeWindow_lock(ANativeWindow *w, ANativeWindow_Buffer *out, void *dirty) {
    (void)w;
    (void)dirty;
    for (int i = 0; i < 8; i++)
        if (g_live_surfaces[i]) {
            g_win_lock_refused++;
            return -22;
        }
    free(g_win_pixels);
    g_win_pixels = (uint32_t *)calloc((size_t)g_win_w * (size_t)g_win_h, 4);
    if (!g_win_pixels)
        return -12;
    out->bits = g_win_pixels;
    out->width = g_win_w;
    out->height = g_win_h;
    out->stride = g_win_w;
    out->format = WINDOW_FORMAT_RGBA_8888;
    g_win_locks++;
    return 0;
}
int ANativeWindow_unlockAndPost(ANativeWindow *w) {
    (void)w;
    g_win_posts++;
    return 0;
}

static uint64_t test_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000u + (uint64_t)(t.tv_nsec / 1000000);
}

#define TEST_STACK_SIZE (1u << 20)
static char test_stack[TEST_STACK_SIZE];
static struct {
    int init_ok;
    int frame1_ok;
    int frame2_ok;
    int frame3_ok;
    int frame4_ok;
    unsigned off_w, off_h, log_w, log_h, off2_w, off2_h;
    unsigned blit_src_w, blit_src_h, blit_dst_w, blit_dst_h;
    unsigned blit2_src_w, blit2_src_h, blit2_dst_w, blit2_dst_h;
    int scale0, scale_miss, scale_probe, scale_rollback, scale_still;
    int scale_hitch, scale_new_window, pin_new_window, scale_reset, pin_reset;
    unsigned probe_off_w, probe_off_h;
    unsigned probe_blit_src_w, probe_blit_src_h, probe_blit_dst_w, probe_blit_dst_h;

    int mode_init, mode_fifo_only, mode_relaxed;
    int creates_before_storm, creates_after_storm, creates_after_change;
    int creates_before_storm2, creates_after_storm2;
    int storm_ok, frame5_ok, frame6_ok;
    int quality_modes_ok;

    unsigned paced_ms;
} g_res;

static struct {
    int fr_strategy_first, fr_plain_first, fr_clear_first;
    ANativeWindow *fr_window_first;
    int lost_ok, no_window_frames_ok, acquires_without_window;
    int attach_ok, frames_after_return_ok;
    int instance_creates_delta, device_creates_delta, pipeline_creates_delta, surface_creates_delta;
    int held_recreate_ok;
    int timeout_skip_ok, timeout_next_ok;
    int surface_lost_ok, surface_lost_device_delta, surface_lost_surface_delta;
    int device_lost_ok, device_lost_device_delta;
    int textures_kept, font_requeued;
    int infinite_acquires;
    int pool_refused_first, pool_retry_ok;
} g_bg;

static int bg_frame(Buffer *b) {
    int ok = ds_graphics_begin_frame(b);
    if (ok) {
        rect(0, 0, 10, 10, 0xff123456);
        ds_graphics_end_frame();
    }
    return ok;
}

static void test_background_cycle(Buffer *b) {
    static int second_window, third_window;
    ANativeWindow *win2 = (ANativeWindow *)&second_window;
    g_present_suboptimal_always = 0;
    g_present_suboptimal_once = 0;
    g_bg.fr_strategy_first = g_frame_rate_strategy_calls;
    g_bg.fr_plain_first = g_frame_rate_calls;
    g_bg.fr_window_first = g_frame_rate_window;

    static uint32_t px[4] = {0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu};
    static Texture tex_kept;
    memset(&tex_kept, 0, sizeof tex_kept);
    tex_kept.name = "kept.png";
    tex_kept.w = 2;
    tex_kept.h = 2;
    tex_kept.pixels = px;
    ds_vk_pending_texture(&tex_kept);
    bg_frame(b);
    int inst0 = g_instance_creates, dev0 = g_device_creates, pipe0 = g_pipeline_creates, surf0 = g_surface_creates;

    ds_graphics_begin_frame(b);
    ds_graphics_cancel_frame();

    ds_graphics_window_lost();
    g_bg.fr_clear_first = g_clear_frame_rate_calls;
    g_dead_window = g_test_window;
    g_bg.lost_ok = (vk_surface == VK_NULL_HANDLE && vk_swap == VK_NULL_HANDLE && vk_device != VK_NULL_HANDLE);

    int acq = g_acquires;
    g_bg.no_window_frames_ok = !ds_graphics_begin_frame(b) && !ds_graphics_begin_frame(b);
    g_bg.acquires_without_window = g_acquires - acq;

    g_bg.attach_ok = ds_graphics_init(NULL, win2);
    int ok = 1;
    for (int i = 0; i < 5; i++) ok &= bg_frame(b);
    g_bg.frames_after_return_ok = ok;
    g_bg.instance_creates_delta = g_instance_creates - inst0;
    g_bg.device_creates_delta = g_device_creates - dev0;
    g_bg.pipeline_creates_delta = g_pipeline_creates - pipe0;
    g_bg.surface_creates_delta = g_surface_creates - surf0;
    g_bg.textures_kept = tex_kept.gpu.uploaded;

    ds_graphics_begin_frame(b);
    ds_graphics_cancel_frame();
    vk_need_recreate = 1;
    g_bg.held_recreate_ok = bg_frame(b) && bg_frame(b);

    g_acquire_next = VK_TIMEOUT;
    g_bg.timeout_skip_ok = !ds_graphics_begin_frame(b);
    g_bg.timeout_next_ok = bg_frame(b);

    int dev1 = g_device_creates, surf1 = g_surface_creates;
    g_present_next = VK_ERROR_SURFACE_LOST_KHR;
    bg_frame(b);
    ok = 1;
    for (int i = 0; i < 3; i++) ok &= bg_frame(b);
    g_bg.surface_lost_ok = ok;
    g_bg.surface_lost_device_delta = g_device_creates - dev1;
    g_bg.surface_lost_surface_delta = g_surface_creates - surf1;

    int dev2 = g_device_creates;
    g_acquire_next = VK_ERROR_DEVICE_LOST;
    ds_graphics_begin_frame(b);
    ok = 1;
    for (int i = 0; i < 3; i++) ok &= bg_frame(b);
    g_bg.device_lost_ok = ok && vk_ready;
    g_bg.device_lost_device_delta = g_device_creates - dev2;
    ds_vk_pending_texture(&tex_kept);
    bg_frame(b);
    g_bg.textures_kept &= tex_kept.gpu.uploaded;
    g_bg.infinite_acquires = g_acquire_infinite;

    static uint32_t px2[4] = {0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu};
    static Texture tex_new;
    memset(&tex_new, 0, sizeof tex_new);
    tex_new.name = "idle.png";
    tex_new.w = 2;
    tex_new.h = 2;
    tex_new.pixels = px2;
    static Texture tex_warm;
    memset(&tex_warm, 0, sizeof tex_warm);
    tex_warm.name = "warm.png";
    tex_warm.w = 2;
    tex_warm.h = 2;
    tex_warm.pixels = px2;
    ds_vk_pending_texture(&tex_warm);
    bg_frame(b);
    size_t pools0 = vk_desc_pool_n;
    g_desc_refuse_pool = pools0 ? vk_desc_pools[pools0 - 1] : VK_NULL_HANDLE;
    ds_vk_pending_texture(&tex_new);
    bg_frame(b);

    g_bg.pool_refused_first = g_desc_refusals == 1;
    for (int i = 0; i < 3; i++) {
        ds_vk_pending_texture(&tex_new);
        bg_frame(b);
    }
    g_bg.pool_refused_first &= g_desc_refusals == 1;
    g_bg.pool_retry_ok = tex_warm.gpu.uploaded && pools0 >= 1 && tex_new.gpu.uploaded && tex_new.gpu.desc &&
                         vk_desc_pool_n == pools0 + 1;
    ds_vk_texture_gpu_release(&tex_warm);
    g_desc_refuse_pool = VK_NULL_HANDLE;
    ds_vk_texture_gpu_release(&tex_new);

    ds_graphics_window_lost();
    g_dead_window = win2;
    (void)third_window;
    ds_vk_texture_gpu_release(&tex_kept);
}

static void *test_thread(void *arg) {
    (void)arg;

    static int dummy_window;
    ANativeWindow *win = (ANativeWindow *)&dummy_window;
    g_test_window = win;

    vk_set_frame_rate = fake_set_frame_rate;
    vk_set_frame_rate_strategy = fake_set_frame_rate_strategy;
    vk_clear_frame_rate = fake_clear_frame_rate;
    g_res.init_ok = ds_graphics_init(NULL, win);
    if (!g_res.init_ok)
        return 0;
    g_res.mode_init = (int)g_created_present_mode;
    Buffer b;
    memset(&b, 0, sizeof b);
    b.width = 720;
    b.height = 1280;
    b.stride = 720;
    g_res.frame1_ok = ds_graphics_begin_frame(&b);
    rect(0, 0, 10, 10, 0xff123456);
    ds_graphics_end_frame();

    g_res.frame2_ok = ds_graphics_begin_frame(&b);
    rect(0, 0, 10, 10, 0xff654321);
    ds_graphics_end_frame();

    g_res.frame3_ok = ds_graphics_begin_frame(&b);
    rect(0, 0, 10, 10, 0xff123456);
    ds_graphics_end_frame();

    /* Switch all user-selectable quality modes at frame boundaries. The fake
     * device advertises 4x support, so MSAA must get a separate multisample
     * attachment; SMAA must get both its target and its post-process pipeline. */
    g_res.quality_modes_ok = 1;
    ds_graphics_set_quality_mode(1);
    g_res.quality_modes_ok &= ds_graphics_begin_frame(&b);
    rect(0, 0, 10, 10, 0xff123456);
    ds_graphics_end_frame();
    g_res.quality_modes_ok &= vk_render_mode == 1 && vk_msaa_samples == VK_SAMPLE_COUNT_4_BIT &&
                              vk_msaa_image != VK_NULL_HANDLE;
    ds_graphics_set_quality_mode(2);
    g_res.quality_modes_ok &= ds_graphics_begin_frame(&b);
    rect(0, 0, 10, 10, 0xff123456);
    ds_graphics_end_frame();
    g_res.quality_modes_ok &= vk_render_mode == 2 && vk_smaa_image != VK_NULL_HANDLE &&
                              vk_smaa_fb != VK_NULL_HANDLE && vk_pipe_smaa != VK_NULL_HANDLE;
    ds_graphics_set_quality_mode(0);
    g_res.quality_modes_ok &= ds_graphics_begin_frame(&b);
    rect(0, 0, 10, 10, 0xff123456);
    ds_graphics_end_frame();
    g_res.quality_modes_ok &= vk_render_mode == 0 && vk_msaa_image == VK_NULL_HANDLE &&
                              vk_smaa_image == VK_NULL_HANDLE;

    g_res.off_w = vk_off_w;
    g_res.off_h = vk_off_h;
    g_res.log_w = vk_log_w;
    g_res.log_h = vk_log_h;
    g_res.blit_src_w = g_blit_src_w;
    g_res.blit_src_h = g_blit_src_h;
    g_res.blit_dst_w = g_blit_dst_w;
    g_res.blit_dst_h = g_blit_dst_h;

    uint64_t pace_t0 = test_ms();
    for (int i = 0; i < 3; i++) {
        if (!ds_graphics_begin_frame(&b))
            break;
        ds_graphics_end_frame();
    }
    g_res.paced_ms = (unsigned)(test_ms() - pace_t0);
    vk_pace_on = 0;
    vk_pace_next_ns = 0;

    g_res.scale0 = ds_graphics_pixel_scale();

    for (int i = 0; i < 100; i++) ds_graphics_report_frame_interval(0.0167);

    for (int i = 0; i < 40; i++) ds_graphics_report_frame_interval(0.150);
    g_res.scale_hitch = ds_graphics_pixel_scale();

    for (int i = 0; i < 30; i++) ds_graphics_report_frame_interval(0.0333);
    g_res.scale_miss = ds_graphics_pixel_scale();
    g_res.frame4_ok = ds_graphics_begin_frame(&b);
    rect(0, 0, 10, 10, 0xff123456);
    ds_graphics_end_frame();
    g_res.off2_w = vk_off_w;
    g_res.off2_h = vk_off_h;
    g_res.blit2_src_w = g_blit_src_w;
    g_res.blit2_src_h = g_blit_src_h;
    g_res.blit2_dst_w = g_blit_dst_w;
    g_res.blit2_dst_h = g_blit_dst_h;

    for (int i = 0; i < 3000; i++) ds_graphics_report_frame_interval(0.0167);
    g_res.scale_probe = ds_graphics_pixel_scale();
    if (!ds_graphics_begin_frame(&b))
        return 0;
    rect(0, 0, 10, 10, 0xff123456);
    ds_graphics_end_frame();
    g_res.probe_off_w = vk_off_w;
    g_res.probe_off_h = vk_off_h;
    g_res.probe_blit_src_w = g_blit_src_w;
    g_res.probe_blit_src_h = g_blit_src_h;
    g_res.probe_blit_dst_w = g_blit_dst_w;
    g_res.probe_blit_dst_h = g_blit_dst_h;
    for (int i = 0; i < 45; i++) ds_graphics_report_frame_interval(0.0167);
    for (int i = 0; i < 10; i++) ds_graphics_report_frame_interval(0.0333);
    g_res.scale_rollback = ds_graphics_pixel_scale();

    for (int i = 0; i < 405; i++) ds_graphics_report_frame_interval(0.0167);
    g_res.scale_still = ds_graphics_pixel_scale();

    gfx_autoscale_new_window();
    g_res.scale_new_window = ds_graphics_pixel_scale();
    g_res.pin_new_window = 0;
    gfx_autoscale_reset();
    g_res.scale_reset = ds_graphics_pixel_scale();
    g_res.pin_reset = 0;

    g_res.creates_before_storm = g_swapchain_creates;
    g_present_suboptimal_always = 1;
    g_res.storm_ok = 1;
    for (int i = 0; i < 150; i++) {
        if (!ds_graphics_begin_frame(&b)) {
            g_res.storm_ok = 0;
            break;
        }
        rect(0, 0, 10, 10, 0xff123456);
        ds_graphics_end_frame();
    }
    g_res.creates_after_storm = g_swapchain_creates;

    g_present_modes[0] = VK_PRESENT_MODE_FIFO_KHR;
    g_present_mode_count = 1;
    g_surface_min_images = 3;
    for (int i = 0; i < 40; i++) {
        g_res.frame5_ok = ds_graphics_begin_frame(&b);
        ds_graphics_end_frame();
    }
    g_res.mode_fifo_only = (int)g_created_present_mode;
    g_res.creates_after_change = g_swapchain_creates;

    g_present_modes[0] = VK_PRESENT_MODE_FIFO_KHR;
    g_present_modes[1] = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    g_present_mode_count = 2;
    g_surface_min_images = 4;
    for (int i = 0; i < 40; i++) {
        g_res.frame6_ok = ds_graphics_begin_frame(&b);
        ds_graphics_end_frame();
    }
    g_res.mode_relaxed = (int)g_created_present_mode;

    g_res.creates_before_storm2 = g_swapchain_creates;
    for (int i = 0; i < 60; i++) {
        ds_graphics_begin_frame(&b);
        ds_graphics_end_frame();
    }
    g_res.creates_after_storm2 = g_swapchain_creates;
    g_present_suboptimal_always = 0;

    test_background_cycle(&b);
    ds_graphics_shutdown();
    return 0;
}

static int check_start_failures(void) {
    int fail = 0, ok;
    static int fb_window, fake_mgr;
    ANativeWindow *win = (ANativeWindow *)&fb_window;
    ds_graphics_shutdown();

    g_sw_images = 10;
    g_acquire_last_image = 1;
    g_present_index_max = -1;
    ok = ds_graphics_init(NULL, win);
    int frame_ok = 0;
    if (ok) {
        Buffer fb = {0};
        fb.width = g_win_w;
        fb.height = g_win_h;
        fb.stride = g_win_w;
        frame_ok = ds_graphics_begin_frame(&fb);
        if (frame_ok) {
            rect(0, 0, 10, 10, 0xff123456);
            ds_graphics_end_frame();
        }
    }
    if (!ok || !frame_ok || g_present_index_max != 9) {
        fail = 1;
        printf("FAIL: a swapchain with 10 images: start %d (reason '%s'), frame %d, presented image %d, expected 9\n",
               ok, ds_graphics_failure(), frame_ok, g_present_index_max);
    }
    ds_graphics_shutdown();
    g_sw_images = 3;
    g_acquire_last_image = 0;

    g_caps_alpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    ok = ds_graphics_init(NULL, win);
    if (!ok || g_created_alpha != VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR) {
        fail = 1;
        printf("FAIL: a surface that lists only INHERIT: start %d, compositeAlpha %d\n", ok, (int)g_created_alpha);
    }
    ds_graphics_shutdown();
    g_caps_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR | VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;

    g_caps_free_extent = 1;
    g_win_w = 800;
    g_win_h = 480;
    ok = ds_graphics_init(NULL, win);
    if (!ok || g_created_extent.width != 800 || g_created_extent.height != 480) {
        fail = 1;
        printf("FAIL: a free currentExtent: start %d, swapchain %ux%u, expected the 800x480 window\n", ok,
               g_created_extent.width, g_created_extent.height);
    }
    ds_graphics_shutdown();
    g_caps_free_extent = 0;

    g_win_w = 1280;
    g_win_h = 720;
    g_real_assets = 1;
    g_swapchain_fail_next = VK_ERROR_INITIALIZATION_FAILED;
    ok = ds_graphics_init((AAssetManager *)(void *)&fake_mgr, win);
    const char *why = ds_graphics_failure();
    if (ok || strcmp(why, "vkCreateSwapchainKHR") != 0) {
        fail = 1;
        printf("FAIL: a failing vkCreateSwapchainKHR: start %d, noted reason '%s'\n", ok, why);
    }
    int shown = ds_graphics_show_failure(win, 4);
    long ink = 0;
    if (g_win_pixels)
        for (long i = 0; i < (long)g_win_w * g_win_h; i++)
            if (g_win_pixels[i] != 0xff100b1cu)
                ink++;
    if (!shown || g_win_locks != 1 || g_win_posts != 1 || g_win_lock_refused || ink < 3000) {
        fail = 1;
        printf("FAIL: failure screen: shown %d, locks %d, posts %d, refused %d, text pixels %ld\n", shown, g_win_locks,
               g_win_posts, g_win_lock_refused, ink);
    }
    if (!strstr(g_log, "vulkan start failure screen: Шаг: vkCreateSwapchainKHR")) {
        fail = 1;
        printf("FAIL: the failure screen text is not in the log\n");
    }
    const char *ppm = getenv("VK_FAILURE_PPM");
    if (ppm && g_win_pixels) {
        FILE *f = fopen(ppm, "wb");
        if (f) {
            fprintf(f, "P6 %d %d 255\n", g_win_w, g_win_h);
            for (long i = 0; i < (long)g_win_w * g_win_h; i++) {
                const uint8_t *px = (const uint8_t *)&g_win_pixels[i];
                fputc(px[0], f);
                fputc(px[1], f);
                fputc(px[2], f);
            }
            fclose(f);
        }
    }
    g_real_assets = 0;

    ok = ds_graphics_init(NULL, win);
    if (!ok || ds_graphics_failure()[0]) {
        fail = 1;
        printf("FAIL: after the failure screen a new start: %d, reason '%s'\n", ok, ds_graphics_failure());
    }
    ds_graphics_shutdown();
    g_win_w = 720;
    g_win_h = 1280;
    return fail;
}

int main(void) {
    memset(test_stack, 0xDE, sizeof test_stack);
    pthread_t th;
    pthread_attr_t attr;
    int fail = 0;
    pthread_attr_init(&attr);
    pthread_attr_setstack(&attr, test_stack, sizeof test_stack);
    pthread_create(&th, &attr, test_thread, NULL);
    pthread_join(th, NULL);
    pthread_attr_destroy(&attr);
    int start_fail = check_start_failures();
    fail |= start_fail;

    if (!g_res.init_ok) {
        fail = 1;
        printf("FAIL: ds_graphics_init returned 0\n");
    }
    if (!g_res.frame1_ok) {
        fail = 1;
        printf("FAIL: begin_frame of the first frame returned 0\n");
    }
    if (!g_res.frame2_ok) {
        fail = 1;
        printf("FAIL: begin_frame of the second frame, after the format change, returned 0\n");
    }
    if (g_swapchain_creates < 2) {
        fail = 1;
        printf("FAIL: expected 2+ swapchain rebuilds, got %d\n", g_swapchain_creates);
    }
    if (g_pipeline_creates < 8) {
        fail = 1;
        printf("FAIL: expected 8+ pipeline creations (4 at init and 4 after the format change), got %d\n",
               g_pipeline_creates);
    }
    if (!g_res.frame3_ok) {
        fail = 1;
        printf("FAIL: begin_frame of the third frame returned 0\n");
    }
    if (!g_res.quality_modes_ok) {
        fail = 1;
        printf("FAIL: pixelation, MSAA and SMAA did not rebuild usable render paths\n");
    }
    if (g_res.paced_ms < 40) {
        fail = 1;
        printf("FAIL: three frames took %u ms, so the 60 fps pace is not held\n", g_res.paced_ms);
    }
    if (g_res.off_w != 720 || g_res.off_h != 1280) {
        fail = 1;
        printf("FAIL: without upscale the offscreen is %ux%u, expected 720x1280\n", g_res.off_w, g_res.off_h);
    }
    if (g_res.log_w != 720 || g_res.log_h != 1280) {
        fail = 1;
        printf("FAIL: logical size %ux%u, expected 720x1280\n", g_res.log_w, g_res.log_h);
    }
    if (fabsf(g_pc[0] - 2.0f / 720.0f) > 1e-9f || fabsf(g_pc[1] - 2.0f / 1280.0f) > 1e-9f) {
        fail = 1;
        printf("FAIL: push constants of the upscaled frame are %g %g, expected %g %g (the shader must divide by the "
               "window, not the offscreen)\n",
               g_pc[0], g_pc[1], 2.0f / 720.0f, 2.0f / 1280.0f);
    }
    if (g_res.blit_src_w != 720 || g_res.blit_src_h != 1280 || g_res.blit_dst_w != 720 || g_res.blit_dst_h != 1280) {
        fail = 1;
        printf("FAIL: blit %dx%d -> %dx%d, expected 720x1280 -> 720x1280 (1:1)\n", g_res.blit_src_w, g_res.blit_src_h,
               g_res.blit_dst_w, g_res.blit_dst_h);
    }
    if (!g_res.frame4_ok) {
        fail = 1;
        printf("FAIL: begin_frame of the frame after the slow frames returned 0\n");
    }
    if (g_res.scale0 != 1) {
        fail = 1;
        printf("FAIL: initial internal resolution is 1/%d, expected 1/1\n", g_res.scale0);
    }
    if (g_res.scale_hitch != 1) {
        fail = 1;
        printf("FAIL: loading hitches coarsened the resolution to 1/%d, expected 1/1\n", g_res.scale_hitch);
    }
    if (g_res.scale_miss != 1) {
        fail = 1;
        printf("FAIL: after two runs of vsync misses the resolution is 1/%d, expected 1/1\n", g_res.scale_miss);
    }
    if (g_res.scale_new_window != 1 || g_res.pin_new_window) {
        fail = 1;
        printf("FAIL: a new window renders at 1/%d (pin %d), expected 1/1 and no pin\n", g_res.scale_new_window,
               g_res.pin_new_window);
    }
    if (g_res.scale_reset != 1 || g_res.pin_reset) {
        fail = 1;
        printf("FAIL: the shutdown reset renders at 1/%d (pin %d), expected 1/1 and no pin\n", g_res.scale_reset,
               g_res.pin_reset);
    }
    if (g_res.off2_w != 720 || g_res.off2_h != 1280) {
        fail = 1;
        printf("FAIL: offscreen after slow frames is %ux%u, expected the full 720x1280 window\n", g_res.off2_w,
               g_res.off2_h);
    }
    if (g_res.blit2_src_w != 720 || g_res.blit2_src_h != 1280 || g_res.blit2_dst_w != 720 || g_res.blit2_dst_h != 1280) {
        fail = 1;
        printf("FAIL: blit after slow frames is %dx%d -> %dx%d, expected 720x1280 -> 720x1280\n", g_res.blit2_src_w,
               g_res.blit2_src_h, g_res.blit2_dst_w, g_res.blit2_dst_h);
    }
    if (g_res.scale_probe != 1) {
        fail = 1;
        printf("FAIL: after a long clean window the resolution is 1/%d, expected 1/1\n", g_res.scale_probe);
    }
    if (g_res.probe_off_w != 720 || g_res.probe_off_h != 1280) {
        fail = 1;
        printf("FAIL: probe offscreen %ux%u, expected the full 720x1280 window\n", g_res.probe_off_w,
               g_res.probe_off_h);
    }
    if (g_res.probe_blit_src_w != 720 || g_res.probe_blit_src_h != 1280 || g_res.probe_blit_dst_w != 720 ||
        g_res.probe_blit_dst_h != 1280) {
        fail = 1;
        printf("FAIL: probe blit %dx%d -> %dx%d, expected 720x1280 -> 720x1280\n", g_res.probe_blit_src_w,
               g_res.probe_blit_src_h, g_res.probe_blit_dst_w, g_res.probe_blit_dst_h);
    }
    if (g_res.scale_rollback != 1) {
        fail = 1;
        printf("FAIL: after more misses the resolution is 1/%d, expected 1/1\n", g_res.scale_rollback);
    }
    if (g_res.scale_still != 1) {
        fail = 1;
        printf("FAIL: after the rollback the resolution moved again (1/%d), expected it to stay at 1/1\n",
               g_res.scale_still);
    }
    if (g_blits < 3) {
        fail = 1;
        printf("FAIL: blit was not called on every frame (%d calls)\n", g_blits);
    }

    if (g_bg.fr_strategy_first != 1 || g_bg.fr_plain_first != 0) {
        fail = 1;
        printf("FAIL: the frame rate request ran %d times with a strategy and %d times without, expected 1 + 0\n",
               g_bg.fr_strategy_first, g_bg.fr_plain_first);
    }
    if (g_frame_rate_value != 60.0f || g_frame_rate_compat != 1 || g_frame_rate_strategy != 1) {
        fail = 1;
        printf("FAIL: the platform is asked for fps=%g, compatibility=%d, strategy=%d; expected 60 / FIXED_SOURCE(1) / "
               "ALWAYS(1)\n",
               g_frame_rate_value, (int)g_frame_rate_compat, (int)g_frame_rate_strategy);
    }
    if (g_bg.fr_window_first != g_test_window) {
        fail = 1;
        printf("FAIL: the frame rate request went to the wrong window\n");
    }
    if (g_bg.fr_clear_first != 1) {
        fail = 1;
        printf("FAIL: the frame rate request was not cleared on window close (%d)\n", g_bg.fr_clear_first);
    }

    if (!g_bg.lost_ok) {
        fail = 1;
        printf("FAIL: after TERM_WINDOW the surface/swapchain are still alive or the device is gone\n");
    }
    if (!g_bg.no_window_frames_ok || g_bg.acquires_without_window) {
        fail = 1;
        printf("FAIL: frames ran without a window (%d acquires)\n", g_bg.acquires_without_window);
    }
    if (!g_bg.attach_ok || !g_bg.frames_after_return_ok) {
        fail = 1;
        printf("FAIL: no frames after the return from the background\n");
    }
    if (g_bg.instance_creates_delta || g_bg.device_creates_delta || g_bg.pipeline_creates_delta) {
        fail = 1;
        printf("FAIL: the return rebuilt the renderer (%d instances, %d devices, %d pipelines), only a surface is "
               "expected\n",
               g_bg.instance_creates_delta, g_bg.device_creates_delta, g_bg.pipeline_creates_delta);
    }
    if (g_bg.surface_creates_delta != 1) {
        fail = 1;
        printf("FAIL: the return made %d surfaces, expected 1\n", g_bg.surface_creates_delta);
    }
    if (!g_bg.held_recreate_ok) {
        fail = 1;
        printf("FAIL: frames failed after a cancelled frame and a swapchain rebuild\n");
    }
    if (!g_bg.timeout_skip_ok || !g_bg.timeout_next_ok) {
        fail = 1;
        printf("FAIL: an acquire timeout was not skipped cleanly\n");
    }
    if (!g_bg.pool_refused_first || !g_bg.pool_retry_ok) {
        fail = 1;
        printf(
            "FAIL: a texture whose descriptor pool refused did not go up from a fresh pool (refused %d, retried %d)\n",
            g_bg.pool_refused_first, g_bg.pool_retry_ok);
    }
    if (g_bg.infinite_acquires) {
        fail = 1;
        printf("FAIL: %d acquires waited forever (UINT64_MAX)\n", g_bg.infinite_acquires);
    }
    if (!g_bg.surface_lost_ok || g_bg.surface_lost_device_delta || g_bg.surface_lost_surface_delta != 1) {
        fail = 1;
        printf("FAIL: SURFACE_LOST recovery (frames %d, devices +%d, surfaces +%d; expected frames, +0, +1)\n",
               g_bg.surface_lost_ok, g_bg.surface_lost_device_delta, g_bg.surface_lost_surface_delta);
    }
    if (!g_bg.device_lost_ok || g_bg.device_lost_device_delta != 1) {
        fail = 1;
        printf("FAIL: DEVICE_LOST recovery (frames %d, devices +%d; expected frames and +1)\n", g_bg.device_lost_ok,
               g_bg.device_lost_device_delta);
    }
    if (!g_bg.textures_kept) {
        fail = 1;
        printf("FAIL: a texture did not survive the background / device rebuild\n");
    }

    if (g_res.mode_init != (int)VK_PRESENT_MODE_MAILBOX_KHR) {
        fail = 1;
        printf("FAIL: the first swapchain was created with presentMode %d, expected MAILBOX(%d)\n", g_res.mode_init,
               (int)VK_PRESENT_MODE_MAILBOX_KHR);
    }
    if (g_res.mode_fifo_only != (int)VK_PRESENT_MODE_FIFO_KHR) {
        fail = 1;
        printf("FAIL: on a FIFO-only panel the chosen mode is %d\n", g_res.mode_fifo_only);
    }
    if (g_res.mode_relaxed != (int)VK_PRESENT_MODE_FIFO_RELAXED_KHR) {
        fail = 1;
        printf("FAIL: with FIFO+FIFO_RELAXED available the chosen mode is %d, expected FIFO_RELAXED(%d)\n",
               g_res.mode_relaxed, (int)VK_PRESENT_MODE_FIFO_RELAXED_KHR);
    }

    if (!g_res.storm_ok) {
        fail = 1;
        printf("FAIL: begin_frame failed during the SUBOPTIMAL storm\n");
    }
    if (g_res.creates_after_storm != g_res.creates_before_storm) {
        fail = 1;
        printf("FAIL: SUBOPTIMAL with an unchanged surface rebuilt the swapchain %d times\n",
               g_res.creates_after_storm - g_res.creates_before_storm);
    }
    if (g_res.creates_after_storm2 != g_res.creates_before_storm2) {
        fail = 1;
        printf("FAIL: the second SUBOPTIMAL storm rebuilt the swapchain %d times\n",
               g_res.creates_after_storm2 - g_res.creates_before_storm2);
    }

    if (g_res.creates_after_change != g_res.creates_before_storm + 1) {
        fail = 1;
        printf("FAIL: after the surface change there were %d rebuilds, expected one\n",
               g_res.creates_after_change - g_res.creates_before_storm);
    }
    if (!g_res.frame5_ok || !g_res.frame6_ok) {
        fail = 1;
        printf("FAIL: the frames after the surface change never started\n");
    }
    if (!strstr(g_log, "SUBOPTIMAL ignored")) {
        fail = 1;
        printf("FAIL: ignoring SUBOPTIMAL is missing from the log, the only trace on a device\n");
    }
    if (!strstr(g_log, "asked the platform for 60 fps")) {
        fail = 1;
        printf("FAIL: the log has no line about asking for 60 fps\n");
    }
    if (!strstr(g_log, "swapchain stale")) {
        fail = 1;
        printf("FAIL: the log does not say why the swapchain was rebuilt\n");
    }
    if (g_violations) {
        fail = 1;
        printf("FAIL: the strict driver caught an invalid create-info: %s\n", g_violation_msg);
    }
    if (!fail) {
        printf("PASS: init, frames and resolution: the game always renders at the panel's own size "
               "(hitches, vsync misses, probes, a new window and a shutdown all stay at 1/1) plus a "
               "swapchain format change; frame steps: SUBOPTIMAL with an unchanged surface does not "
               "rebuild the "
               "swapchain, the present mode goes MAILBOX -> FIFO_RELAXED -> FIFO, and the platform "
               "is asked for 60 fps once per window; pNext and flags are clean, %d pipelines "
               "created\n",
               g_pipeline_creates);
        printf("PASS: background and return keep the device (one new surface, no new instance/device/"
               "pipelines), no frames without a window, a cancelled frame plus a rebuild never "
               "re-acquires into a signalled semaphore, acquire has a finite timeout and a timeout "
               "skips the frame, SURFACE_LOST gets a new surface and DEVICE_LOST a new device\n");
        printf("PASS: start failures: a swapchain with 10 images (VK_INCOMPLETE on the Xiaomi), an INHERIT-only "
               "surface and a free currentExtent start, a failed "
               "start notes its step and draws it on the window with the CPU after Vulkan let go of "
               "it, and the next window starts Vulkan again\n");
    }
    return fail;
}
