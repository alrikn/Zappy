/*
 * src/shaders/triangle.frag — Fragment shader for the hardcoded RGB triangle.
 *
 * Responsibility: receive the interpolated colour from the vertex shader and
 *                 write it to the framebuffer as the final pixel colour.
 *
 * Architecture: called once per rasterised fragment (one per covered pixel).
 *               The GPU interpolates 'fragColor' between the three vertex colours
 *               using barycentric coordinates before invoking this shader.
 *               Output is written to the single colour attachment at location 0,
 *               which corresponds to the swapchain image in the framebuffer.
 */

#version 450

/*
 * layout(location = 0) in: receive the interpolated colour from triangle.vert.
 * The location index must match the 'out' declaration in the vertex shader.
 * The GPU has already performed linear interpolation across the triangle — this
 * value is a blend of the three vertex colours weighted by proximity to each vertex.
 */
layout(location = 0) in vec3 fragColor;

/*
 * layout(location = 0) out: write the final colour to the first colour attachment.
 * In the render pass, attachment 0 is the swapchain image. The GPU writes this
 * vec4 to the corresponding pixel in the framebuffer.
 * The .w component (alpha) is set to 1.0 (fully opaque).
 */
layout(location = 0) out vec4 outColor;

void main()
{
    /*
     * Pass the interpolated RGB colour through, with full opacity (alpha = 1.0).
     * No lighting, no texture sampling, no post-processing — pure interpolation.
     * The result is the characteristic RGB gradient triangle used throughout
     * Vulkan tutorials as proof that the full pipeline is working end-to-end.
     */
    outColor = vec4(fragColor, 1.0);
}
