/**
 * @file camera/Camera.hpp
 * @brief Free-fly first-person camera: position, yaw, pitch, view and projection matrices.
 * @details Pure math class with zero Vulkan includes. Lives in src/camera/ to keep
 *          camera logic separate from both the renderer plumbing and the game world state.
 *          The Renderer owns one Camera instance and calls processMouseDelta() and
 *          processKeyboard() each frame, then reads viewMatrix() and projMatrix() to fill
 *          the UboMvp before writing it to the uniform buffer.
 *
 *          Architecture: Camera has no Vulkan dependency — it only uses GLM. It could live
 *          in world/ (which also has no Vulkan), but camera state is a rendering concept
 *          (the virtual eye), not a game-world concept (tiles, players, resources), so
 *          src/camera/ is a cleaner separation.
 */

#pragma once

#include <glm/glm.hpp>

/**
 * @brief First-person free-fly camera that produces view and projection matrices for the UBO.
 * @details Coordinate conventions (right-hand, Y-up):
 *            - World +X points right.
 *            - World +Y points up.
 *            - World -Z is the default forward direction (camera looks toward -Z at yaw=0).
 *          Yaw rotates around the world Y axis (left/right turn).
 *          Pitch rotates around the camera's local X axis (up/down tilt), clamped to
 *          [-89 deg, +89 deg] to prevent gimbal lock and camera inversion.
 *
 *          Mouse input: raw GLFW cursor pixel deltas multiplied by kSensitivity.
 *          Movement input: WASD booleans multiplied by kSpeed * dt. Movement is confined
 *          to the XZ plane (no vertical translation in this feature).
 *
 *          Projection convention: the Y component of the projection matrix is negated to
 *          flip Vulkan's clip-space Y axis. GLM's glm::perspective follows OpenGL convention
 *          (Y-up clip space); Vulkan expects Y-down. Negating projection[1][1] corrects this
 *          without requiring the GLM_FORCE_DEPTH_ZERO_TO_ONE workaround at the matrix level.
 *
 *          Lifetime: created in the Renderer constructor (one instance, owned by Renderer
 *          via unique_ptr<Camera>). Destroyed in the Renderer destructor — no destruction
 *          order constraint because Camera holds no Vulkan handles.
 *
 *          Non-copyable, non-movable — camera state is mutable and must not be duplicated.
 *          Thread-safety: not thread-safe. All calls from the main thread only.
 */
class Camera {
public:
    /**
     * @brief Initialise the camera at the default position and orientation.
     * @details Position = (0, 0, 3). Yaw = 0 (looking toward -Z). Pitch = 0 (level).
     *          FOV = 45 degrees. Near = 0.1, Far = 100.0.
     *          The projection matrix is computed once here and cached because the window
     *          is not resizable (GLFW_RESIZABLE = GLFW_FALSE); it never needs to change.
     * @param aspectRatio Width divided by height of the render target (e.g. 1280.0f / 720.0f).
     *                    Must be positive and non-zero.
     * @throws CameraException if aspectRatio is zero or negative.
     */
    explicit Camera(float aspectRatio);

    ~Camera() = default;

    Camera(const Camera&)            = delete; ///< Non-copyable.
    Camera& operator=(const Camera&) = delete; ///< Non-copyable.
    Camera(Camera&&)                 = delete; ///< Non-movable.
    Camera& operator=(Camera&&)      = delete; ///< Non-movable.

    /**
     * @brief Apply a raw cursor pixel delta to update yaw and pitch.
     * @details Multiplies the delta by kSensitivity before applying. Yaw increases when the
     *          mouse moves right (positive dx). Pitch decreases when the mouse moves down
     *          (positive dy in GLFW convention where Y increases downward), so that moving
     *          the mouse downward tilts the camera down as expected. Pitch is clamped to
     *          [-89, +89] degrees after each update to prevent camera inversion.
     * @param dx Horizontal pixel delta (positive = moved right in GLFW coords).
     * @param dy Vertical pixel delta (positive = moved down in GLFW coords).
     */
    void processMouseDelta(float dx, float dy);

    /**
     * @brief Translate the camera position based on WASD key states and elapsed time.
     * @details Movement is along the camera's forward and right vectors projected onto
     *          the XZ plane (no vertical movement in this feature). The camera speed is
     *          kSpeed world units per second, scaled by the frame delta time dt.
     * @param forward  True if the W key is currently pressed — move toward -Z (at yaw=0).
     * @param backward True if the S key is currently pressed — move toward +Z.
     * @param left     True if the A key is currently pressed — strafe left.
     * @param right    True if the D key is currently pressed — strafe right.
     * @param dt       Frame delta time in seconds. Scales the movement so speed is
     *                 consistent regardless of frame rate.
     */
    void processKeyboard(bool forward, bool backward, bool left, bool right, float dt);

    /**
     * @brief Compute and return the view matrix for this frame.
     * @details Builds the view matrix using glm::lookAt from the current position,
     *          the look-at target (position + forward direction derived from yaw and pitch),
     *          and the world-up vector (0, 1, 0). Called once per frame before writing the UBO.
     * @return Column-major 4x4 view matrix transforming from world space to camera space.
     */
    [[nodiscard]] glm::mat4 viewMatrix() const;

    /**
     * @brief Return the precomputed projection matrix.
     * @details Perspective projection with a 45-degree FOV, computed once at construction
     *          and cached. The projection[1][1] component is negated to correct Vulkan's
     *          inverted clip-space Y axis compared to OpenGL. GLM_FORCE_DEPTH_ZERO_TO_ONE
     *          is defined as a CMake compile definition, so glm::perspective outputs
     *          depth in [0, 1] (Vulkan convention) rather than [-1, 1] (OpenGL convention).
     * @return Column-major 4x4 projection matrix transforming from camera space to clip space.
     */
    [[nodiscard]] glm::mat4 projMatrix() const;

private:
    float     _yaw{0.0f};                      ///< Horizontal rotation angle in degrees.
    float     _pitch{0.0f};                    ///< Vertical rotation angle in degrees, clamped to [-89, +89].
    glm::vec3 _position{0.0f, 0.0f, 3.0f};    ///< World-space camera origin.
    glm::mat4 _proj{1.0f};                     ///< Precomputed projection matrix; constant for the window lifetime.

    static constexpr float kSensitivity{0.1f}; ///< Mouse pixel delta to degrees multiplier.
    static constexpr float kSpeed{5.0f};        ///< Camera movement speed in world units per second.
    static constexpr float kFov{45.0f};         ///< Vertical field of view in degrees.
    static constexpr float kNear{0.1f};         ///< Near clip plane distance (world units).
    static constexpr float kFar{100.0f};        ///< Far clip plane distance (world units).
};
