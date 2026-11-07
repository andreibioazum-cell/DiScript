/* Renderer entry point: Vulkan on Android, software rasterizer on PC. */
#define RGB(r,g,b) ((uint32_t)(r)<<16 | (uint32_t)(g)<<8 | (uint32_t)(b))
#include "engine/graphics/types.inc"
#include "engine/graphics/geometry.inc"
#include "engine/graphics/text.inc"
#ifdef CB_PC
#include "platform/pc/gfx_soft.inc"
#else
#include "engine/graphics/vulkan_backend.inc"
#endif
#include "engine/graphics/fx_filters.inc"
#include "engine/graphics/autoscale.inc"
#include "engine/graphics/asset_pack.inc"
#include "engine/graphics/lifecycle.inc"
#include "engine/graphics/text_rainbow.inc"
#include "engine/graphics/tex_round.inc"
#ifdef CB_PC
#include "platform/pc/pc_record.inc"
#include "engine/graphics/frame_lifecycle.inc"
#else
#include "engine/graphics/frame_lifecycle.inc"
#include "engine/graphics/fallback_screen.inc"
#endif
