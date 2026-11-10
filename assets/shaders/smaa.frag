#version 450

/* Compact single-pass morphological AA.  It detects contrast around the
 * current pixel, then blends only along the local edge tangent. */
layout(binding = 0) uniform sampler2D u_tex;
layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 out_color;

float luma(vec3 color) {
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

void main() {
    vec4 center = texture(u_tex, v_uv);
    vec4 north = textureOffset(u_tex, v_uv, ivec2(0, -1));
    vec4 south = textureOffset(u_tex, v_uv, ivec2(0, 1));
    vec4 west = textureOffset(u_tex, v_uv, ivec2(-1, 0));
    vec4 east = textureOffset(u_tex, v_uv, ivec2(1, 0));

    float horizontal = abs(luma(west.rgb) - luma(east.rgb));
    float vertical = abs(luma(north.rgb) - luma(south.rgb));
    float contrast = max(horizontal, vertical);
    float edge = smoothstep(0.045, 0.24, contrast);
    /* Blend across the staircase's tangent, never blur a flat area. */
    vec4 tangent = horizontal > vertical ? (north + south) * 0.5 : (west + east) * 0.5;
    out_color = mix(center, tangent, edge * 0.30);
}
