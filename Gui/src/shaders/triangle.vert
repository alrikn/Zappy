/**
 * @file src/shaders/triangle.vert
 * @brief Vertex shader for the two-triangle MVP camera demo.
 * @details Transforms six world-space vertex positions by the view and projection matrices
 *          read from the UboMvp uniform buffer at set=0, binding=0. The result is a
 *          correct perspective-projected clip-space position for each vertex.
 *
 *          Moving the camera (WASD + mouse look) changes the view matrix each frame,
 *          which in turn changes where the triangles appear on screen — proving the full
 *          CPU-to-GPU MVP transform path works end-to-end.
 *
 *          Called by the graphics pipeline for every vkCmdDraw(6, 1, 0, 0) call.
 *          No vertex buffer is bound — all positions and colours are embedded in the shader
 *          using gl_VertexIndex as a selector. This eliminates vertex buffer management and
 *          keeps the feature focused on the UBO and camera transform plumbing.
 *
 *          Triangle A (vertices 0-2): RGB gradient. Positioned in the XY plane at Z=0,
 *          spanning X in [-0.5, 0.5] and Y in [-0.5, 0.5]. The camera is at (0,0,3)
 *          looking toward -Z, so the triangles face the camera directly and are visible.
 *
 *          Triangle B (vertices 3-5): solid cyan. Shifted slightly right and behind
 *          Triangle A (higher Z value = farther from the camera). The depth test ensures
 *          Triangle A correctly occludes Triangle B where they overlap.
 *
 *          Key fix vs. the previous XZ-plane layout: vertices that all share Y=0 with a
 *          camera also at Y=0 produce a degenerate edge-on view — the triangle has zero
 *          projected screen area and is invisible. Triangles in the XY plane (with varying
 *          X and Y, constant Z) are fully face-on to a camera looking along -Z and are
 *          rendered correctly.
 */

#version 450

/**
 * layout(set=0, binding=0) uniform UboMvp:
 *   Matches the VkDescriptorSetLayout created in Pipeline — one uniform buffer binding at
 *   set=0, binding=0, visible to the vertex stage. The CPU writes view+proj matrices into
 *   the corresponding UniformBuffer each frame before the draw call is submitted.
 *
 *   Physically: before each draw call the GPU reads this binding's VkDescriptorBufferInfo
 *   (buffer handle + offset + range) from the descriptor set. During vertex shader execution
 *   it fetches the 128-byte UboMvp struct from that buffer into shader registers. All 6
 *   vertex shader invocations in the draw call read the same buffer contents (the matrices
 *   are frame-constant), which is why uniform buffers are the correct tool here (as opposed
 *   to storage buffers, which are per-element writeable).
 *
 *   std140 layout: each mat4 is 64 bytes (4 columns × vec4 = 4×16 bytes). The struct size
 *   is exactly 128 bytes. std140 ensures no padding is inserted between view and proj, and
 *   the CPU-side UboMvp struct (in UboMvp.hpp) uses glm::mat4 which matches this layout.
 */
layout(set = 0, binding = 0) uniform UboMvp {
    mat4 view; ///< World-space → camera-space transform (written by Camera::viewMatrix()).
    mat4 proj; ///< Camera-space → clip-space transform (written by Camera::projMatrix()).
} ubo;

/**
 * layout(location = 0) out: pass the colour to the fragment shader.
 * The location index must match the 'in' declaration in triangle.frag.
 * The GPU linearly interpolates this value across the triangle surface
 * between the three vertices (Gouraud interpolation).
 */
layout(location = 0) out vec3 fragColor;

/**
 * World-space XY-plane positions for six vertices (two triangles).
 *
 * Coordinate system: right-hand, Y-up.
 *   X = right, Y = up, Z = toward the viewer (camera looks toward -Z at yaw=0).
 *
 * The camera is at (0, 0, 3) looking toward -Z. Triangles placed in the XY plane
 * (Z constant, X and Y varying) face the camera directly and have non-zero projected
 * screen area — they are fully visible.
 *
 * Previous layout (all Y=0, XZ plane) was incorrect: a flat XZ-plane triangle
 * viewed by a camera also at Y=0 is edge-on (zero projected area → invisible).
 *
 * Triangle A (vertices 0-2): centred at Z=0, spanning X and Y around the origin.
 *   Closer to the camera (lower Z value when viewed = smaller camera-space Z distance
 *   in a right-hand -Z forward system) so it wins the depth test over Triangle B.
 *
 * Triangle B (vertices 3-5): shifted right and placed at Z=-0.5 (farther from camera).
 *   Partially overlaps Triangle A. The depth test discards Triangle B fragments that
 *   are hidden behind Triangle A.
 *
 * With depth testing ON and a perspective projection from (0,0,3):
 *   Triangle A occludes Triangle B where they overlap because its world-space Z (0.0)
 *   is closer to the camera (at Z=3) than Triangle B's Z (-0.5).
 *   Depth values emerge naturally from the perspective divide of the projection result —
 *   no hardcoded constants needed.
 */
const vec3 positions[6] = vec3[](
    // Triangle A — RGB gradient, in the XY plane at Z=0 (closest to camera)
    vec3( 0.0,  0.5, 0.0),  // vertex 0: top-centre
    vec3( 0.5, -0.5, 0.0),  // vertex 1: bottom-right
    vec3(-0.5, -0.5, 0.0),  // vertex 2: bottom-left

    // Triangle B — solid cyan, in the XY plane at Z=-0.5 (farther from camera)
    // Shifted right so it partially overlaps Triangle A; the depth test resolves the overlap.
    vec3( 0.8,  0.5, -0.5),  // vertex 3: top-right
    vec3( 1.3, -0.5, -0.5),  // vertex 4: bottom-far-right
    vec3( 0.3, -0.5, -0.5)   // vertex 5: bottom-near-right
);

/**
 * Per-vertex colours.
 * Triangle A (vertices 0-2): classic RGB gradient (red/green/blue at each vertex).
 * Triangle B (vertices 3-5): solid cyan (0, 1, 1) — visually distinct from the
 *   gradient so it is immediately obvious which shape is in front in the overlap.
 */
const vec3 colors[6] = vec3[](
    // Triangle A
    vec3(1.0, 0.0, 0.0),  // red   (vertex 0)
    vec3(0.0, 1.0, 0.0),  // green (vertex 1)
    vec3(0.0, 0.0, 1.0),  // blue  (vertex 2)

    // Triangle B — all cyan
    vec3(0.0, 1.0, 1.0),  // cyan (vertex 3)
    vec3(0.0, 1.0, 1.0),  // cyan (vertex 4)
    vec3(0.0, 1.0, 1.0)   // cyan (vertex 5)
);

void main()
{
    /*
     * gl_VertexIndex: built-in, set by the GPU to the current vertex index (0-5).
     *
     * MVP transform: proj * view * position.
     *   1. vec4(positions[gl_VertexIndex], 1.0): promote the 3D world-space position
     *      to homogeneous coordinates (W=1). W=1 means "this is a point, not a direction".
     *   2. ubo.view * ...: transform from world space to camera space.
     *      The view matrix moves the world so the camera is at the origin looking toward -Z.
     *      Physically: this is equivalent to moving all geometry relative to a fixed camera.
     *   3. ubo.proj * ...: transform from camera space to clip space.
     *      Applies perspective foreshortening: things farther away get smaller X and Y values.
     *      After the perspective divide (done automatically by the GPU after the vertex shader),
     *      the result is a normalised device coordinate (NDC) position.
     *      The Y flip is already baked into ubo.proj (projection[1][1] is negated in Camera.cpp)
     *      so this produces correct Vulkan clip-space Y without any per-vertex correction.
     *
     * The depth value written to the depth buffer is no longer a hardcoded constant.
     * It emerges naturally from the perspective divide of the clip-space Z component.
     * Vertices farther from the camera produce higher clip-space Z (after divide), so
     * the depth test automatically resolves occlusion based on actual world-space distance.
     *
     * Skipping the view multiply: the camera would never appear to move — all geometry
     * would stay fixed in camera space regardless of the CPU-side camera position.
     * Skipping the proj multiply: there would be no perspective foreshortening and no
     * correct depth values — the render would look like an orthographic projection.
     */
    gl_Position = ubo.proj * ubo.view * vec4(positions[gl_VertexIndex], 1.0);
    fragColor   = colors[gl_VertexIndex];
}
