// High-speed rounded gradient fills, color space conversions, and spring smoothing.

#include "cpp/ds_cpp.h"
#include <math.h>
#include <stdint.h>

extern "C" void rect(float x, float y, float w, float h, uint32_t color);

namespace {

constexpr uint32_t kAlphaMask = 0xFF000000u;

inline uint32_t channel_of(uint32_t color, int shift) {
    return (color >> shift) & 0xFFu;
}

inline uint32_t clamp_u8(float v) {
    if (v <= 0.0f) return 0u;
    if (v >= 255.0f) return 255u;
    return static_cast<uint32_t>(v + 0.5f);
}

uint32_t shade_argb(uint32_t color, float k) {
    uint32_t a = (color >> 24) & 0xFFu;
    if (a == 0) a = 255u;
    return (a << 24) |
           (clamp_u8(static_cast<float>(channel_of(color, 16)) * k) << 16) |
           (clamp_u8(static_cast<float>(channel_of(color, 8)) * k) << 8) |
           clamp_u8(static_cast<float>(channel_of(color, 0)) * k);
}

inline float mix_f(float a, float b, float t) {
    return a + (b - a) * t;
}

inline float row_inset(float dy, float rad) {
    float d = fabsf(dy);
    if (d >= rad) return rad;
    return rad - sqrtf(rad * rad - d * d);
}

}  // namespace

extern "C" uint32_t ds_lerp_argb(uint32_t c0, uint32_t c1, float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    uint32_t a0 = (c0 >> 24) ? ((c0 >> 24) & 0xFFu) : 255u;
    uint32_t a1 = (c1 >> 24) ? ((c1 >> 24) & 0xFFu) : 255u;
    uint32_t a = clamp_u8(mix_f(static_cast<float>(a0), static_cast<float>(a1), t));
    uint32_t r = clamp_u8(mix_f(static_cast<float>(channel_of(c0, 16)), static_cast<float>(channel_of(c1, 16)), t));
    uint32_t g = clamp_u8(mix_f(static_cast<float>(channel_of(c0, 8)), static_cast<float>(channel_of(c1, 8)), t));
    uint32_t b = clamp_u8(mix_f(static_cast<float>(channel_of(c0, 0)), static_cast<float>(channel_of(c1, 0)), t));
    return (a << 24) | (r << 16) | (g << 8) | b;
}

extern "C" uint32_t ds_hsv_to_argb(float h, float s, float v, float a) {
    h = h - floorf(h);
    if (s < 0.0f) s = 0.0f;
    if (s > 1.0f) s = 1.0f;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    if (a < 0.0f) a = 0.0f;
    if (a > 1.0f) a = 1.0f;

    float c = v * s;
    float x = c * (1.0f - fabsf(fmodf(h * 6.0f, 2.0f) - 1.0f));
    float m = v - c;
    float r = 0.0f, g = 0.0f, b = 0.0f;
    int sector_i = static_cast<int>(h * 6.0f);
    switch (sector_i % 6) {
        case 0: r = c; g = x; b = 0; break;
        case 1: r = x; g = c; b = 0; break;
        case 2: r = 0; g = c; b = x; break;
        case 3: r = 0; g = x; b = c; break;
        case 4: r = x; g = 0; b = c; break;
        default: r = c; g = 0; b = x; break;
    }
    return (clamp_u8(a * 255.0f) << 24) |
           (clamp_u8((r + m) * 255.0f) << 16) |
           (clamp_u8((g + m) * 255.0f) << 8) |
           clamp_u8((b + m) * 255.0f);
}

extern "C" void ds_panel_gradient(float x, float y, float w, float h, float rad, uint32_t top_col, uint32_t bot_col) {
    if (w <= 0.0f || h <= 0.0f) return;
    if (rad < 0.0f) rad = 0.0f;
    if (rad > h * 0.5f) rad = h * 0.5f;
    if (rad > w * 0.5f) rad = w * 0.5f;

    const float cy_top = y + rad;
    const float cy_bot = y + h - rad;
    const float y_end = y + h;
    const int first = static_cast<int>(floorf(y));
    const int last = static_cast<int>(ceilf(y_end));

    // Coalesce interior straight rows into 2px strips when height is large to cut command count.
    const int step = (h > 140.0f) ? 2 : 1;
    for (int py = first; py < last; ) {
        float rc_probe = static_cast<float>(py) + 0.5f;
        int cur_step = (rc_probe > cy_top + 1.0f && rc_probe < cy_bot - 2.0f) ? step : 1;
        float r0 = fmaxf(static_cast<float>(py), y);
        float r1 = fminf(static_cast<float>(py + cur_step), y_end);
        py += cur_step;
        if (r1 <= r0) continue;
        float rc = 0.5f * (r0 + r1);
        float t = (h > 1.0f) ? (rc - y) / h : 0.0f;
        uint32_t c = ds_lerp_argb(top_col, bot_col, t);
        float inset = 0.0f;
        if (rad > 0.0f) {
            if (rc < cy_top) inset = row_inset(rc - cy_top, rad);
            else if (rc > cy_bot) inset = row_inset(rc - cy_bot, rad);
        }
        float sw = w - 2.0f * inset;
        if (sw > 0.0f) {
            rect(x + inset, r0, sw, r1 - r0, c);
        }
    }
}

extern "C" void cb_button_gradient(float x, float y, float w, float h, float rad, uint32_t color) {
    uint32_t top = shade_argb(color, 1.18f);
    uint32_t bot = shade_argb(color, 0.78f);
    ds_panel_gradient(x, y, w, h, rad, top, bot);
}

extern "C" float ds_smooth_damp(float current, float target, float *velocity, float smooth_time, float dt_sec) {
    if (smooth_time < 0.0001f) smooth_time = 0.0001f;
    float omega = 2.0f / smooth_time;
    float x = omega * dt_sec;
    float exp_k = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);
    float change = current - target;
    float temp = (*velocity + omega * change) * dt_sec;
    *velocity = (*velocity - omega * temp) * exp_k;
    return target + (change + temp) * exp_k;
}
