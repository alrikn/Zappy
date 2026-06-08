/*
 * src/shaders/triangle.vert — Vertex shader for the two-triangle depth-test demo.
 *
 * Responsibility: emit six vertices (two triangles) in clip space, each at a
 *                 distinct depth (Z) value, so the depth test has two different
 *                 depth values to compare. The fragment shader receives the
 *                 per-vertex colour via the 'fragColor' varying.
 *
 * Architecture: called by the graphics pipeline for every vkCmdDraw(6, 1, 0, 0)
 *               call. No vertex buffer is bound — all positions, depths, and
 *               colours are embedded in the shader using gl_VertexIndex as a
 *               selector. This eliminates vertex buffer management, VMA allocation,
 *               and staging buffer complexity, keeping the depth-buffer feature
 *               focused purely on the depth attachment plumbing.
 *
 * Two-triangle layout (screen space, Y-axis points DOWN in Vulkan):
 *
 *   Triangle A (vertices 0-2): RGB gradient, centred, Z = 0.3 (nearer)
 *     Vertex 0: top centre      ( 0.0, -0.5)
 *     Vertex 1: bottom right    ( 0.5,  0.5)
 *     Vertex 2: bottom left     (-0.5,  0.5)
 *
 *   Triangle B (vertices 3-5): solid cyan, offset right-and-down, Z = 0.7 (farther)
 *     Vertex 3: top left        ( 0.0, -0.1)
 *     Vertex 4: bottom right    ( 0.7,  0.6)
 *     Vertex 5: top right       ( 0.7, -0.6)
 *
 * Expected visual result with depth testing ON:
 *   In the overlap region (lower-right of triangle A, upper-left of triangle B),
 *   triangle A (Z=0.3, nearer) occludes triangle B (Z=0.7, farther).
 *   Outside the overlap, each triangle's full colour is visible.
 *
 *   If depth testing were OFF, the draw-order would determine the winner:
 *   triangle B would overwrite triangle A in the overlap region because it is
 *   submitted second (gl_VertexIndex 3-5 come after 0-2).
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
 * Hardcoded clip-space XY positions for six vertices (two triangles).
 * Clip space: X in [-1, 1], Y in [-1, 1], Z in [0, 1], W = 1.
 * Y is flipped vs OpenGL: positive Y points DOWN in Vulkan's clip space.
 *
 * Vertices 0-2 form Triangle A (RGB gradient, Z=0.3 — nearer to camera).
 * Vertices 3-5 form Triangle B (solid cyan, Z=0.7 — farther from camera).
 *
 * Triangle B is positioned so its upper-left corner overlaps Triangle A's
 * lower-right region. That overlap area is where the depth test is visible:
 * Triangle A's fragments (Z=0.3 < Z=0.7) beat Triangle B's (Z=0.7) and
 * the RGB gradient colour wins over cyan in the overlap pixels.
 */
const vec2 positions[6] = vec2[](
    // Triangle A — nearer (Z=0.3)
    vec2( 0.0, -0.5),   // vertex 0: top centre
    vec2( 0.5,  0.5),   // vertex 1: bottom right
    vec2(-0.5,  0.5),   // vertex 2: bottom left

    // Triangle B — farther (Z=0.7), overlaps Triangle A's lower-right corner
    vec2( 0.0, -0.1),   // vertex 3: top left  (overlaps triangle A's interior)
    vec2( 0.7,  0.6),   // vertex 4: bottom right
    vec2( 0.7, -0.6)    // vertex 5: top right
);

/*
 * Clip-space depth (Z) for each vertex.
 * Vulkan's depth range is [0, 1] after perspective divide.
 * Z=0 is the near plane, Z=1 is the far plane.
 * All three vertices of each triangle share the same Z so the triangle is
 * parallel to the screen — making the depth comparison unambiguous.
 *
 * The depth buffer is cleared to 1.0 at the start of each frame.
 * With VK_COMPARE_OP_LESS, a fragment passes if its depth < stored depth.
 *   - Triangle A vertices: Z=0.3 < 1.0 → pass on first draw.
 *   - Triangle B vertices: Z=0.7 < 1.0 → pass where A has not yet written.
 *   - Triangle B vertices: Z=0.7 > 0.3 → FAIL where A already wrote Z=0.3.
 */
const float depths[6] = float[](
    // Triangle A — nearer
    0.3,   // vertex 0
    0.3,   // vertex 1
    0.3,   // vertex 2

    // Triangle B — farther
    0.7,   // vertex 3
    0.7,   // vertex 4
    0.7    // vertex 5
);

/*
 * Per-vertex colours.
 * Triangle A (vertices 0-2): classic RGB gradient (red/green/blue at each vertex).
 * Triangle B (vertices 3-5): solid cyan (0, 1, 1) — visually distinct from the
 *   gradient so it is immediately obvious which shape is in front in the overlap.
 */
const vec3 colors[6] = vec3[](
    // Triangle A
    vec3(1.0, 0.0, 0.0),   // red   (vertex 0 — top centre)
    vec3(0.0, 1.0, 0.0),   // green (vertex 1 — bottom right)
    vec3(0.0, 0.0, 1.0),   // blue  (vertex 2 — bottom left)

    // Triangle B — all cyan
    vec3(0.0, 1.0, 1.0),   // cyan (vertex 3)
    vec3(0.0, 1.0, 1.0),   // cyan (vertex 4)
    vec3(0.0, 1.0, 1.0)    // cyan (vertex 5)
);

void main()
{
    /*
     * gl_VertexIndex: built-in variable set by the GPU to the current vertex index.
     * With vkCmdDraw(6, 1, 0, 0):
     *   Invocations 0-2 → Triangle A (RGB gradient, Z=0.3).
     *   Invocations 3-5 → Triangle B (solid cyan, Z=0.7).
     *
     * gl_Position: built-in output; the clip-space position for this vertex.
     * vec4(x, y, z, 1.0): W=1 (no perspective divide for this flat demo).
     * The Z component is taken from the depths[] array so each triangle
     * sits at a distinct depth. The GPU writes Z to the depth buffer during
     * the EARLY_FRAGMENT_TESTS stage and compares it against the stored value.
     */
    gl_Position = vec4(positions[gl_VertexIndex], depths[gl_VertexIndex], 1.0);
    fragColor   = colors[gl_VertexIndex];
}
