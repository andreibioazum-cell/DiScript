#define VK_USE_PLATFORM_ANDROID_KHR
#include <vulkan/vulkan.h>
#include <time.h>
#ifdef __ANDROID__
#include <dlfcn.h>
#endif
#include "engine/graphics/shaders_spirv.inc"

#define DS_VK_MAX_SW_IMAGES 32
#define DS_VK_FRAMES_IN_FLIGHT 2
#define DS_VK_DESC_SETS_PER_POOL 96

static char vk_fail_step[96];
static int vk_fail_code;
static char vk_diag_gpu[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE];
static uint32_t vk_diag_api, vk_diag_driver, vk_diag_vendor;

static void ds_vk_note_failure(const char *what, int code) {
    if (vk_fail_step[0])
        return;
    snprintf(vk_fail_step, sizeof(vk_fail_step), "%s", what ? what : "?");
    vk_fail_code = code;
}

static int ds_vk_check_result(VkResult r, const char *what) {
    if (r == VK_SUCCESS)
        return 1;
    ds_log_err("vulkan: %s failed (%d)", what, (int)r);
    ds_vk_note_failure(what, (int)r);
    return 0;
}

static VkInstance vk_instance;
static VkSurfaceKHR vk_surface;
static ANativeWindow *vk_window;
static VkPhysicalDevice vk_phys;
static VkDevice vk_device;
static VkQueue vk_queue;
static uint32_t vk_queue_family;
static int vk_ready;
static int vk_need_recreate;
static int vk_blit_ok = 1;


static VkSwapchainKHR vk_swap;
static uint32_t vk_sw_count;
static VkImage vk_sw_images[DS_VK_MAX_SW_IMAGES];
static VkImageView vk_sw_views[DS_VK_MAX_SW_IMAGES];
static VkSemaphore vk_sw_present[DS_VK_MAX_SW_IMAGES];
static VkFormat vk_sw_format = VK_FORMAT_R8G8B8A8_UNORM;
static VkExtent2D vk_sw_extent;
static uint32_t vk_sw_index;
static int vk_image_held;

static VkPresentModeKHR vk_sw_present_mode = VK_PRESENT_MODE_FIFO_KHR;
static VkSurfaceTransformFlagBitsKHR vk_sw_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
static VkColorSpaceKHR vk_sw_colorspace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
static uint32_t vk_sw_want_images;
static int vk_sw_stale;
static int vk_sw_check_timer;
static unsigned vk_sw_recreates;
static unsigned vk_sw_ignored;

#define DS_VK_TARGET_FPS 60
#define DS_VK_FRAME_RATE_FIXED_SOURCE 1
#define DS_VK_FRAME_RATE_ALWAYS 1
typedef int32_t (*DsVkSetFrameRateFn)(ANativeWindow *window, float frameRate, int8_t compatibility);
typedef int32_t (*DsVkSetFrameRateStrategyFn)(ANativeWindow *window, float frameRate, int8_t compatibility,
                                              int8_t changeFrameRateStrategy);
typedef void (*DsVkClearFrameRateFn)(ANativeWindow *window);
static DsVkSetFrameRateFn vk_set_frame_rate;
static DsVkSetFrameRateStrategyFn vk_set_frame_rate_strategy;
static DsVkClearFrameRateFn vk_clear_frame_rate;

static uint64_t vk_pace_next_ns;
static int vk_pace_on = 1;

static VkRenderPass vk_renderpass;
static VkFormat vk_rp_format;
static VkPipelineLayout vk_layout_solid;
static VkPipelineLayout vk_layout_image;
static VkPipelineLayout vk_layout_tint;
static VkPipeline vk_pipe_solid;
static VkPipeline vk_pipe_image;
static VkPipeline vk_pipe_tint;

static VkPipeline vk_pipe_replace;
static VkDescriptorSetLayout vk_desc_layout;
static VkSampler vk_sampler;

static VkImage vk_off_image;
static VkDeviceMemory vk_off_mem;
static VkImageView vk_off_view;
static VkFramebuffer vk_off_fb;
static VkFormat vk_off_format = VK_FORMAT_R8G8B8A8_UNORM;
static uint32_t vk_off_w, vk_off_h;

static uint32_t vk_log_w, vk_log_h;

typedef struct {
    VkCommandPool pool;
    VkCommandBuffer cmd;
    VkFence fence;
    VkSemaphore image_available;
    VkBuffer vbuf, ibuf;
    VkDeviceMemory vmem, imem;
    GeoVert *vmap;
    uint16_t *imap;
    size_t vcap, icap;
    int submitted;
} DsVkFrame;
static DsVkFrame vk_frames[DS_VK_FRAMES_IN_FLIGHT];
static int vk_frame_cur;

static int vk_begun;

static VkDescriptorPool *vk_desc_pools;
static size_t vk_desc_pool_n, vk_desc_pool_cap, vk_desc_pool_used;

static VkCommandPool vk_upload_pool;
static VkCommandBuffer vk_upload_cmd;
static VkFence vk_upload_fence;

static VkImage vk_font_image;
static VkDeviceMemory vk_font_memory;
static VkImageView vk_font_view;
static VkDescriptorSet vk_font_desc;

#define DS_VK_PENDING_CAP 64
static Texture *vk_pending_tex[DS_VK_PENDING_CAP];
static int vk_pending_n;
static int vk_pending_font;

typedef struct {
    int pipeline;
    const void *desc;
    uint32_t first;
    uint32_t count;
    int sx, sy, sw, sh;
} GeoBatch;
static GeoBatch *vk_batches;
static size_t vk_batch_n, vk_batch_cap;

static int vk_surface_lost;
static int vk_device_lost;
static ANativeWindow *vk_rebuild_window;
static unsigned vk_rebuild_tries;
static unsigned vk_reattaches;

#define DS_VK_ACQUIRE_TIMEOUT_NS 250000000ull
static int ds_vk_attach_window(ANativeWindow *window);
static void ds_vk_surface_lost(void);
static int ds_vk_recover(void);
static int ds_vk_reset_acquire_semaphores(void);

static int ds_vk_create_frame_resources(void);
static void ds_vk_destroy_frame_resources(void);
static int ds_vk_recreate_swapchain(void);
static void ds_vk_destroy_swapchain(void);
static int ds_vk_swapchain_stale(void);
static const char *ds_vk_present_mode_name(VkPresentModeKHR mode);
static void ds_vk_lookup_frame_rate_api(void);
static int ds_vk_request_frame_rate(ANativeWindow *window);
static void ds_vk_release_frame_rate(void);
static int ds_vk_ensure_offscreen(uint32_t w, uint32_t h, VkFormat format);
static void ds_vk_destroy_offscreen(void);
