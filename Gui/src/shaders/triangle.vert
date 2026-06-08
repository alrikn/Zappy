/*
 * src/shaders/triangle.vert — Vertex shader for the hardcoded RGB triangle.
 *
 * Responsibility: emit three vertices in clip space and assign a distinct
 *                 RGB colour to each one. The fragment shader receives the
 *                 interpolated colour via the 'fragColor' varying.
 *
 * Architecture: called by the graphics pipeline for every vkCmdDraw(3, 1, 0, 0)
 *               call. No vertex buffer is bound — all positions and colours are
 *               embedded in the shader using gl_VertexIndex as a selector.
 *               This eliminates vertex buffer management, VMA allocation, and
 *               staging buffer complexity from the bootstrap feature.
 */

#version 450

/*
 * layout(location = 0) out: pass the colour to the fragment shader.
 * The location index must match the 'in' declaration in triangle.frag.
 * The GPU linearly interpolates this value across the triangle surface
 * between the three vertices (Gouraud interpolation).
 */
layout(location = 0) out vec3 fragColor;

/*
 * Hardcoded clip-space positions for three vertices.
 * Clip space: X in [-1, 1], Y in [-1, 1], Z in [0, 1], W = 1.
 * Y is flipped vs OpenGL: positive Y points DOWN in Vulkan's clip space.
 *
 * Vertex 0: top centre    ( 0.0, -0.5) → top of screen
 * Vertex 1: bottom right  ( 0.5,  0.5) → bottom right
 * Vertex 2: bottom left   (-0.5,  0.5) → bottom left
 *
 * Winding order: in Vulkan framebuffer space (Y-axis points down), the
 * sequence 0→1→2 traces a counter-clockwise path, giving a positive signed
 * area (a = 1.0 by the Vulkan spec formula).  The pipeline's frontFace is
 * set to VK_FRONT_FACE_COUNTER_CLOCKWISE, so positive-area triangles are
 * front-facing and this triangle passes the VK_CULL_MODE_BACK_BIT test.
 */
const vec2 positions[3] = vec2[](
    vec2( 0.0, -0.5),   // top centre
    vec2( 0.5,  0.5),   // bottom right
    vec2(-0.5,  0.5)    // bottom left
);

/*
 * Per-vertex colours: red, green, blue.
 * The GPU interpolates between these across the triangle surface.
 */
const vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),  // red   (top centre)
    vec3(0.0, 1.0, 0.0),  // green (bottom right)
    vec3(0.0, 0.0, 1.0)   // blue  (bottom left)
);

void main()
{
    /*
     * gl_VertexIndex: built-in variable set by the GPU to the current vertex index.
     * With vkCmdDraw(3, 1, 0, 0):
     *   First  invocation: gl_VertexIndex = 0 → top centre,    red
     *   Second invocation: gl_VertexIndex = 1 → bottom right,  green
     *   Third  invocation: gl_VertexIndex = 2 → bottom left,   blue
     *
     * gl_Position: built-in output; the clip-space position for this vertex.
     * vec4(x, y, 0.0, 1.0): Z=0 (flat on the near plane), W=1 (no perspective divide).
     */
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragColor   = colors[gl_VertexIndex];
}
