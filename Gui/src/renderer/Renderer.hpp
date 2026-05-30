/**
 * @file Renderer.hpp
 * @brief Vulkan rendering subsystem.
 * @details Responsibility: own the Vulkan instance, device, swapchain, render passes,
 *          pipelines, and descriptor sets. Each frame it reads a FrameData snapshot
 *          from WorldState and submits GPU commands to draw the world.
 *
 *          Placeholder until the renderer bootstrap feature is implemented. The class
 *          exists so main.cpp compiles; no Vulkan objects are created yet.
 *
 *          Architecture position:
 *          main() creates one Renderer. The GLFW window event loop calls
 *          Renderer::drawFrame() once per iteration (stub: no-op for now).
 *          The Renderer reads FrameData (a snapshot of WorldState) — it never
 *          touches WorldState directly to avoid holding a lock during rendering.
 */

#pragma once

/**
 * @brief Vulkan rendering subsystem.
 * @details Lifetime: created once in main() after the GLFW window exists (so the
 *          window handle can be passed to the Vulkan surface creation call later).
 *          Destroyed before glfwTerminate().
 *
 *          Non-copyable: Vulkan objects are non-copyable by nature (handles are
 *          opaque 64-bit integers backed by GPU-side allocations).
 */
class Renderer {
public:
    /**
     * @brief Stub constructor — takes no arguments for now.
     * @details When the Vulkan bootstrap feature is implemented, this will accept
     *          a GLFWwindow* to create the VkSurfaceKHR.
     */
    Renderer() = default;

    ~Renderer() = default;

    Renderer(const Renderer&) = delete;             ///< Non-copyable: Vulkan handles cannot be duplicated.
    Renderer& operator=(const Renderer&) = delete;  ///< Non-copyable: Vulkan handles cannot be duplicated.

    Renderer(Renderer&&) = delete;             ///< Non-movable until concrete Vulkan members are added.
    Renderer& operator=(Renderer&&) = delete;  ///< Non-movable until concrete Vulkan members are added.

    /**
     * @brief Called once per event loop iteration.
     * @details Stub: no-op until the Vulkan renderer is implemented.
     *          Will eventually: acquire swapchain image → record command buffer →
     *          submit to graphics queue → present.
     */
    void drawFrame() {}
};
