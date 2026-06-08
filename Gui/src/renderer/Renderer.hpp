/**
 * @file renderer/Renderer.hpp
 * @brief Top-level Vulkan rendering subsystem — owns all Vulkan objects.
 * @details Renderer is the single entry point for all rendering operations.
 *          It constructs and destroys the complete Vulkan object graph in the correct
 *          order, and drives the per-frame acquire → record → submit → present loop.
 *
 *          Construction order (objects created first are destroyed last):
 *            1. DeviceContext      — VkInstance + VkPhysicalDevice + VkDevice + VkQueue
 *            2. WindowContext      — VkSurfaceKHR (borrowed GLFWwindow*)
 *            3. AllocatorContext   — VmaAllocator (wraps vmaCreateAllocator)
 *            4. DepthResources     — VkImage + VmaAllocation + VkImageView (depth buffer)
 *            5. Pipeline           — VkRenderPass + VkPipelineLayout + VkPipeline
 *            6. SwapchainContext   — VkSwapchainKHR + VkImageViews + VkFramebuffers
 *            7. FrameSyncPool      — imageAvailable VkSemaphores + VkFences
 *            8. renderFinished VkSemaphores (inline member, one per swapchain image)
 *            9. VkCommandPool + VkCommandBuffers (inline members, managed in destructor)
 *
 *          Destruction order (reverse of construction, required by Vulkan):
 *            command buffers/pool → FrameSyncPool → SwapchainContext → Pipeline →
 *            DepthResources → AllocatorContext → WindowContext → DeviceContext
 *
 *          C++ destroys non-static data members in REVERSE declaration order.
 *          To achieve the required destruction sequence with unique_ptr members,
 *          declare them so that reverse-order matches what Vulkan needs:
 *            _device    declared first  → destroyed last  (instance must outlive all)
 *            _allocator declared after _window → destroyed before _window
 *            _depth     declared after _allocator → destroyed before _allocator
 *            _pipeline  declared after _depth → destroyed before _depth
 *            _swapchain declared after _pipeline → destroyed before _pipeline
 *            _syncPool  declared last among unique_ptrs → destroyed first
 *
 *          See Renderer.cpp constructor comment for the full rationale.
 */

#pragma once

#include <cstddef>
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include "renderer/device/DeviceContext.hpp"
#include "renderer/device/AllocatorContext.hpp"
#include "renderer/scene/DepthResources.hpp"
#include "renderer/window/WindowContext.hpp"
#include "renderer/pipeline/Pipeline.hpp"
#include "renderer/window/SwapchainContext.hpp"
#include "renderer/sync/FrameSync.hpp"

/**
 * @brief Owns all Vulkan objects and drives the per-frame render loop.
 * @details Lifetime: created once in main() after glfwInit() and glfwCreateWindow().
 *          Destroyed before glfwTerminate(). The GLFWwindow passed to the constructor
 *          must outlive this object.
 *          Non-copyable, non-movable.
 *
 *          IMPORTANT — member destruction order:
 *          C++ destroys members in REVERSE declaration order. The members below are
 *          declared so that C++ reverse-destruction matches the required Vulkan teardown
 *          sequence:
 *
 *            Declared order → Destroyed order (reversed):
 *              1. _device     → destroyed LAST  (instance must outlive all children)
 *              2. _window     → destroyed 7th   (surface before instance)
 *              3. _allocator  → destroyed 6th   (VmaAllocator before VkDevice/VkInstance)
 *              4. _depth      → destroyed 5th   (depth image before VmaAllocator)
 *              5. _pipeline   → destroyed 4th   (render pass before framebuffers)
 *              6. _swapchain  → destroyed 3rd   (framebuffers before render pass + depth view)
 *              7. _syncPool   → destroyed 2nd   (semaphores/fences before device)
 *              8. cmd pool / buffers (inline) → destroyed 1st (managed in destructor body)
 *
 *          Thread-safety: not thread-safe. All calls must come from the main thread.
 */
class Renderer {
public:
    /**
     * @brief Initialise the entire Vulkan stack: instance, device, swapchain, pipeline,
     *        sync primitives, and command buffers.
     * @param window Non-owning pointer to the GLFW window created in main(). The window
     *               must outlive this Renderer object (main() guarantees this ordering).
     * @throws RendererInitException or RendererVkException on any failure.
     */
    explicit Renderer(GLFWwindow* window);

    /**
     * @brief Wait for all in-flight GPU work to finish, then destroy all Vulkan objects.
     * @details Calls vkDeviceWaitIdle once — permitted only at shutdown (rule 9).
     *          Command pool and buffers are destroyed explicitly here before the
     *          unique_ptr members are destroyed by C++ member destruction.
     */
    ~Renderer();

    Renderer(const Renderer&)            = delete; ///< Non-copyable.
    Renderer& operator=(const Renderer&) = delete; ///< Non-copyable.
    Renderer(Renderer&&)                 = delete; ///< Non-movable.
    Renderer& operator=(Renderer&&)      = delete; ///< Non-movable.

    /**
     * @brief Submit and present one frame: acquire → record → submit → present.
     * @details Does NOT call vkDeviceWaitIdle. Uses per-frame fences and semaphores
     *          to ensure the CPU and GPU do not step on each other's data.
     *          Called once per event loop iteration from main().
     * @throws RendererVkException on a fatal Vulkan error during acquisition or presentation.
     */
    void drawFrame();

private:
    /**
     * @brief Create a command pool and allocate one command buffer per swapchain image.
     * @details Command buffers are re-recorded every frame. Allocating once and re-recording
     *          is more efficient than allocating and freeing each frame.
     */
    void allocateCommandBuffers();

    /**
     * @brief Record the draw commands for one swapchain image index.
     * @details Records: begin render pass (clear colour + depth) → set viewport →
     *          set scissor → bind pipeline (depth test ON) → draw 6 vertices
     *          (two overlapping triangles at different depths) → end render pass.
     * @param imageIndex Index of the swapchain image whose framebuffer to target.
     */
    void recordCommandBuffer(uint32_t imageIndex);

    /// Number of frames to pipeline between CPU recording and GPU execution.
    static constexpr std::size_t kMaxFramesInFlight{2};

    /// Instance + physical device + logical device.
    /// Declared first so that C++ reverse-destruction destroys it LAST — the VkInstance
    /// must outlive every child object (surface, allocator, swapchain, pipeline, sync).
    std::unique_ptr<DeviceContext>    _device;

    /// VkSurfaceKHR owner. Declared second → destroyed second-to-last (surface before instance).
    std::unique_ptr<WindowContext>    _window;

    /// VmaAllocator owner.
    /// Declared after _window → destroyed before _window but after all allocations it manages.
    /// MUST be declared before _depth: the VmaAllocator must outlive every VmaAllocation
    /// made through it. C++ reverse-destruction will destroy _depth before _allocator.
    std::unique_ptr<AllocatorContext> _allocator;

    /// Depth image + GPU memory (VmaAllocation) + VkImageView.
    /// Declared after _allocator → destroyed before _allocator.
    /// Declared before _pipeline because Pipeline needs _depth->format() to build
    /// the render pass depth attachment description.
    std::unique_ptr<DepthResources>   _depth;

    /// VkRenderPass + VkPipelineLayout + VkPipeline.
    /// Declared after _depth → destroyed before _depth.
    /// Must outlive SwapchainContext (framebuffers reference the render pass).
    std::unique_ptr<Pipeline>         _pipeline;

    /// VkSwapchainKHR + colour views + framebuffers.
    /// Declared after _pipeline → destroyed before _pipeline (framebuffers before render pass).
    /// Also destroyed before _depth (framebuffers reference the depth view owned by _depth).
    std::unique_ptr<SwapchainContext> _swapchain;

    /// Per-frame semaphores + fences.
    /// Declared last among unique_ptrs → destroyed first by C++ (before swapchain and device).
    /// VkCommandPool and VkCommandBuffers are destroyed manually in ~Renderer() before
    /// any unique_ptr member destructor runs.
    std::unique_ptr<FrameSyncPool>    _syncPool;

    VkCommandPool                _commandPool{VK_NULL_HANDLE}; ///< Pool for all command buffers.
    std::vector<VkCommandBuffer> _commandBuffers;              ///< One per swapchain image.

    /// "Render finished" semaphores, one per swapchain image (NOT per frame-in-flight).
    /// Indexed by the imageIndex returned from vkAcquireNextImageKHR. This semaphore is
    /// signalled by vkQueueSubmit and waited on by vkQueuePresentKHR for that same image,
    /// so it must stay associated with the image rather than cycling through a smaller
    /// frame-in-flight pool — otherwise it can be reused while still pending on the
    /// presentation engine (VUID-vkQueueSubmit-pSignalSemaphores-00067).
    /// Created/destroyed manually (raw handles), like _commandPool.
    std::vector<VkSemaphore> _renderFinishedSemaphores;

    std::size_t _currentFrame{0}; ///< Cycles [0, kMaxFramesInFlight); selects the active sync slot.
};
