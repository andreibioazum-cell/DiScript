/* Frame-rate independent animation primitives used by the editor UI. */

#include "engine/anim.h"

#include <math.h>

/* Largest step integrated in one go: after a stall (the app was suspended, a
 * file dialog opened) a huge dt would make every panel snap into place. */
#define ANIM_MAX_DT 0.1f

static float clamp01(float t) {
    if (t < 0.0f)
        return 0.0f;
    if (t > 1.0f)
        return 1.0f;
    return t;
}

static float clamp_dt(float dt) {
    if (!(dt > 0.0f))
        return 0.0f;
    return dt > ANIM_MAX_DT ? ANIM_MAX_DT : dt;
}

float anim_approach(float current, float target, float speed, float dt) {
    if (speed <= 0.0f)
        return current;
    if (speed >= 1.0f)
        return target;
    dt = clamp_dt(dt);
    if (dt <= 0.0f)
        return current;
    /* Exponential decay: the same fraction of the remaining gap closes per
     * second no matter how the frame time is chopped up. */
    float f = 1.0f - expf(-speed * 12.0f * dt);
    return current + (target - current) * f;
}

float anim_spring(float current, float *vel, float target, float stiffness, float dt) {
    if (!vel)
        return target;
    dt = clamp_dt(dt);
    if (dt <= 0.0f)
        return current;
    if (stiffness <= 0.0f)
        stiffness = 120.0f;
    /* Critically damped: damping = 2*sqrt(stiffness), so it settles without
     * oscillating but still accelerates instead of easing linearly. */
    float damping = 2.0f * sqrtf(stiffness);
    /* Fixed sub-steps keep the integration stable for high stiffness values. */
    const float h = 1.0f / 240.0f;
    float remaining = dt;
    while (remaining > 0.0f) {
        float step = remaining < h ? remaining : h;
        float accel = (target - current) * stiffness - (*vel) * damping;
        *vel += accel * step;
        current += (*vel) * step;
        remaining -= step;
    }
    return current;
}

float anim_ease_out_cubic(float t) {
    t = clamp01(t);
    float u = 1.0f - t;
    return 1.0f - u * u * u;
}

float anim_ease_in_out_cubic(float t) {
    t = clamp01(t);
    if (t < 0.5f)
        return 4.0f * t * t * t;
    float u = -2.0f * t + 2.0f;
    return 1.0f - (u * u * u) * 0.5f;
}

float anim_ease_out_back(float t) {
    t = clamp01(t);
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    float u = t - 1.0f;
    return 1.0f + c3 * u * u * u + c1 * u * u;
}

float anim_step(float t, float seconds, float dt) {
    dt = clamp_dt(dt);
    if (dt <= 0.0f)
        return t;
    if (seconds <= 0.0f)
        return 1.0f;
    return clamp01(t + dt / seconds);
}

float anim_pulse(float seconds, float period) {
    if (period <= 0.0f)
        return 0.0f;
    float phase = seconds / period;
    phase -= floorf(phase);
    /* 0 -> 1 -> 0 */
    return phase < 0.5f ? phase * 2.0f : (1.0f - phase) * 2.0f;
}
