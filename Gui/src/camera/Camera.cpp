/**
 * @file camera/Camera.cpp
 * @brief Implementation of Camera: GLM-based free-fly first-person camera.
 * @details The camera tracks two angles (yaw and pitch) and a 3D position. Each frame:
 *
 *            1. processMouseDelta() converts raw GLFW cursor pixel deltas into yaw/pitch changes.
 *            2. processKeyboard() translates the camera along its forward/right vectors in XZ.
 *            3. viewMatrix() constructs a glm::lookAt matrix from the resulting pose.
 *            4. projMatrix() returns the cached perspective matrix (built once at construction).
 *
 *          The only mathematical subtlety here is Vulkan's clip-space Y flip. GLM was written
 *          for OpenGL, where the Y axis points upward in clip space. In Vulkan it points
 *          downward. Negating projection[1][1] is the minimal correct fix: it flips the Y
 *          coordinate of every projected vertex so that the triangle winding appears correct
 *          on screen without requiring changes to any vertex data.
 *
 *          GLM_FORCE_DEPTH_ZERO_TO_ONE is set as a CMake compile definition, making
 *          glm::perspective output depth in Vulkan's [0, 1] convention rather than
 *          OpenGL's [-1, 1]. Without it, depth values would map incorrectly and objects
 *          at mid-depth would appear at wrong Z positions in the depth buffer.
 *
 *          Architecture: no Vulkan headers are included here. The only dependency is GLM.
 */

#include "camera/Camera.hpp"
#include "exceptions.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>

// ─── constructor ──────────────────────────────────────────────────────────────

/**
 * @brief Initialise camera state and precompute the projection matrix.
 * @param aspectRatio Width / height of the render target.
 */
Camera::Camera(float aspectRatio)
{
    if (aspectRatio <= 0.0f) {
        throw CameraException("Camera: aspect ratio must be positive, got " +
                              std::to_string(aspectRatio));
    }

    // glm::perspective: build a standard perspective projection matrix.
    //
    // Arguments:
    //   glm::radians(kFov) — vertical field of view converted from degrees to radians.
    //     GLM_FORCE_RADIANS is not set, so glm::perspective expects radians. We convert
    //     explicitly with glm::radians() to avoid any ambiguity.
    //   aspectRatio — width / height. A 1280×720 window gives 16/9 ≈ 1.777.
    //   kNear — the near clip plane. Any geometry closer than 0.1 units is clipped.
    //   kFar  — the far clip plane. Any geometry farther than 100.0 units is clipped.
    //
    // GLM_FORCE_DEPTH_ZERO_TO_ONE (defined in CMakeLists.txt compile_definitions):
    //   makes glm::perspective produce depth in [0, 1] matching Vulkan's convention.
    //   Without it, depth would be in [-1, 1] (OpenGL convention) and the depth test
    //   would behave incorrectly — geometry at the near plane would map to depth -1.0,
    //   which lies outside Vulkan's [0, 1] depth range and would be clipped entirely.
    _proj = glm::perspective(glm::radians(kFov), aspectRatio, kNear, kFar);

    // Vulkan clip-space Y flip:
    //
    // GLM generates projection matrices for OpenGL, where the clip-space Y axis points
    // upward (Y=+1 is the top of the screen). In Vulkan, Y=+1 is the bottom of the
    // screen. Without correction, triangles appear upside-down and back-face culling
    // selects the wrong faces.
    //
    // The fix: negate projection[1][1]. This is the element that maps camera-space Y
    // into clip-space Y. Negating it flips the Y direction so Vulkan's convention is
    // satisfied without changing any geometry data.
    //
    // Alternative fix: GLM_FORCE_LEFT_HANDED + GLM_FORCE_DEPTH_ZERO_TO_ONE, but that
    // changes the handedness of the entire coordinate system which affects cross products,
    // winding order, and every other matrix operation. Negating [1][1] is surgical and
    // only affects the clip-space Y mapping.
    _proj[1][1] *= -1.0f;

    spdlog::debug("Camera: constructed (aspectRatio={:.3f}, FOV={}deg, near={}, far={}).",
                  aspectRatio, kFov, kNear, kFar);
}

// ─── input processing ─────────────────────────────────────────────────────────

/**
 * @brief Apply a raw cursor pixel delta to yaw and pitch.
 * @param dx Horizontal pixel delta (positive = moved right).
 * @param dy Vertical pixel delta (positive = moved down in GLFW coords).
 */
void Camera::processMouseDelta(float dx, float dy)
{
    // kSensitivity converts raw pixel counts into degrees of rotation.
    // At 0.1 deg/pixel, moving the mouse 90 pixels horizontally rotates the camera 9 degrees.
    _yaw += dx * kSensitivity;

    // GLFW cursor Y increases downward. Moving the mouse down (positive dy) should tilt
    // the camera downward, which means decreasing pitch (pitch=0 is level, pitch=-90 is
    // looking straight down). Subtracting dy achieves the correct inversion.
    _pitch -= dy * kSensitivity;

    // Clamp pitch to [-89, +89] degrees.
    // At exactly ±90 degrees, the forward and up vectors become parallel — lookAt would
    // produce a degenerate matrix. Clamping to ±89 prevents this while still allowing
    // nearly-straight-up and nearly-straight-down looks.
    _pitch = std::clamp(_pitch, -89.0f, 89.0f);
}

/**
 * @brief Translate the camera along its forward/right vectors in the XZ plane.
 * @param forward  W key — move forward.
 * @param backward S key — move backward.
 * @param left     A key — strafe left.
 * @param right    D key — strafe right.
 * @param dt       Frame delta time in seconds.
 */
void Camera::processKeyboard(bool forward, bool backward, bool left, bool right, float dt)
{
    // Derive the world-space forward direction from yaw only (no pitch component):
    // Moving forward should slide the camera horizontally, not up or down as if
    // flying. This "horizontal forward" is the camera's look direction projected
    // onto the XZ plane and re-normalised.
    const float yawRad = glm::radians(_yaw);
    const glm::vec3 flatForward = glm::normalize(
        glm::vec3(std::sin(yawRad), 0.0f, -std::cos(yawRad)));

    // Right vector: perpendicular to forward in the XZ plane (rotate forward 90° around Y).
    const glm::vec3 flatRight = glm::normalize(
        glm::cross(flatForward, glm::vec3(0.0f, 1.0f, 0.0f)));

    // Accumulate movement from all active keys. Multiple keys pressed simultaneously
    // combine additively (e.g. W+D = forward-right diagonal).
    glm::vec3 movement{0.0f};
    if (forward)  movement += flatForward;
    if (backward) movement -= flatForward;
    if (right)    movement += flatRight;
    if (left)     movement -= flatRight;

    // Only normalise if there is actual movement — normalising a zero vector is undefined.
    if (glm::length(movement) > 0.0f) {
        // Normalise so diagonal movement (W+D) has the same speed as cardinal movement (W alone).
        _position += glm::normalize(movement) * kSpeed * dt;
    }
}

// ─── matrix accessors ─────────────────────────────────────────────────────────

/**
 * @brief Compute and return the view matrix from current position and orientation.
 * @return Column-major 4x4 view matrix (world space → camera space).
 */
glm::mat4 Camera::viewMatrix() const
{
    // Convert yaw and pitch from degrees to radians for trigonometry.
    const float yawRad   = glm::radians(_yaw);
    const float pitchRad = glm::radians(_pitch);

    // Compute the forward direction vector from yaw and pitch using spherical coordinates.
    //
    // At yaw=0, pitch=0:
    //   forward = (sin(0)*cos(0), sin(0), -cos(0)*cos(0)) = (0, 0, -1)
    //   This is "looking toward -Z" — the default camera direction.
    //
    // At yaw=90:
    //   forward = (sin(90)*cos(0), sin(0), -cos(90)*cos(0)) = (1, 0, 0)
    //   This is "looking toward +X" — turned 90 degrees to the right.
    //
    // At pitch=45:
    //   forward.y = sin(45) ≈ 0.707  (looking upward at 45 degrees)
    //
    // The direction vector is not unit-length in general, but glm::lookAt normalises
    // its direction implicitly, so we can pass it unnormalised.
    const glm::vec3 forward{
        std::sin(yawRad) * std::cos(pitchRad),  // X component
        std::sin(pitchRad),                      // Y component (up/down tilt)
       -std::cos(yawRad) * std::cos(pitchRad),  // Z component (-Z is default forward)
    };

    // glm::lookAt: construct the view matrix from three vectors.
    //   eye    = camera position in world space.
    //   center = the point the camera is looking at (eye + forward direction).
    //   up     = the world up vector — always (0, 1, 0) for a camera that never rolls.
    //
    // Physically: lookAt builds an orthonormal basis from right, up, and -forward,
    // then packs it into a matrix that rotates world coordinates into camera-relative
    // coordinates (the forward axis becomes -Z in camera space).
    //
    // Skipping this and using an identity matrix would mean the camera never moves —
    // every vertex would be rendered in world space with no camera transform.
    return glm::lookAt(_position, _position + forward, glm::vec3(0.0f, 1.0f, 0.0f));
}

/**
 * @brief Return the precomputed projection matrix.
 * @return Column-major 4x4 projection matrix (camera space → clip space).
 */
glm::mat4 Camera::projMatrix() const
{
    return _proj;
}
