#ifndef DISCRIPT_ANIM_H
#define DISCRIPT_ANIM_H

#ifdef __cplusplus
extern "C" {
#endif

/* Small animation toolkit. Every function takes the frame delta explicitly so
 * motion is driven by the host's clock (and by the harness's fixed step in
 * tests) rather than by wall time read in the middle of a frame. */

/* Advances `current` toward `target`. `speed` is roughly how fast it closes the
 * gap per second; >= 1.0 snaps. Frame-rate independent. */
float anim_approach(float current, float target, float speed, float dt);

/* Critically damped spring toward `target`. `*vel` carries velocity between
 * calls. Settles without overshoot but accelerates instead of easing linearly. */
float anim_spring(float current, float *vel, float target, float stiffness, float dt);

/* Easing curves on a 0..1 input. */
float anim_ease_out_cubic(float t);
float anim_ease_in_out_cubic(float t);
float anim_ease_out_back(float t);

/* Advances a 0..1 progress by `dt` toward 1 over `seconds`. */
float anim_step(float t, float seconds, float dt);

/* Smooth 0 -> 1 -> 0 oscillation with the given period in seconds. */
float anim_pulse(float seconds, float period);

#ifdef __cplusplus
}
#endif

#endif /* DISCRIPT_ANIM_H */
