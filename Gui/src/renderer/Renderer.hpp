/**
 * @file renderer/Renderer.hpp
 * @brief Top-level Vulkan rendering subsystem — owns all Vulkan objects and the camera.
 * @details Renderer is the single entry point for all rendering operations.
 *          It constructs and destroys the complete Vulkan object graph in the correct
 *          order, and drives the per-frame acquire → record → submit → present loop.
 *
 *          Construction order (objects created first are destroyed last):
 *            1. DeviceContext      — VkInstance + VkPhysicalDevice + VkDevice + VkQueue
 *            2. WindowContext      — VkSurfaceKHR (borrowed GLFWwindow*)
 *            3. AllocatorContext   — VmaAllocator (wraps vmaCreateAllocator)
 *            4. DepthResources     — VkImage + VmaAllocation + VkImageView (depth buffer)
 *            5. Pipeline           — VkDescriptorSetLayout + VkRenderPass + VkPipelineLayout + VkPipeline
 *            6. SwapchainContext   — VkSwapchainKHR + VkImageViews + VkFramebuffers
 *            7. FrameSyncPool      — imageAvailable VkSemaphores + VkFences
 *            8. UniformBuffers     — one per frame-in-flight, host-coherent VMA buffers
 *            9. DescriptorAllocator — VkDescriptorPool + per-frame VkDescriptorSets
 *           10. Camera             — pure-math camera; no Vulkan objects
 *           11. renderFinished VkSemaphores (inline member, one per swapchain image)
 *           12. VkCommandPool + VkCommandBuffers (inline members, managed in destructor)
 *
 *          Destruction order (reverse of construction, required by Vulkan):
 *            command buffers/pool → renderFinished semaphores → Camera →
 *            DescriptorAllocator → UniformBuffers → FrameSyncPool →
 *            SwapchainContext → Pipeline → DepthResources →
 *            AllocatorContext → WindowContext → DeviceContext
 *
 *          C++ destroys non-static data members in REVERSE declaration order.
 *          To achieve the required destruction sequence with unique_ptr members,
 *          declare them so that reverse-order matches what Vulkan needs.
 *          See the member declarations below for the rationale at each position.
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
#include "renderer/scene/UniformBuffer.hpp"
#include "renderer/scene/DescriptorAllocator.hpp"
#include "camera/Camera.hpp"

/**
 * @brief Owns all Vulkan objects, the camera, and drives the per-frame render loop.
 * @details Lifetime: created once in main() after glfwInit() and glfwCreateWindow().
 *          Destroyed before glfwTerminate(). The GLFWwindow passed to the constructor
 *          must outlive this object (main() declaration order guarantees this).
 *          Non-copyable, non-movable.
 *
 *          IMPORTANT — member destruction order:
 *          C++ destroys members in REVERSE declaration order. The members below are
 *          declared so that C++ reverse-destruction matches the required Vulkan teardown
 *          sequence:
 *
 *            Declared order → Destroyed order (reversed):
 *              1. _device              → destroyed LAST   (instance must outlive all children)
 *              2. _window              → destroyed 11th   (surface before instance)
 *              3. _allocator           → destroyed 10th   (VmaAllocator before VkDevice/VkInstance)
 *              4. _depth               → destroyed 9th    (depth image before VmaAllocator)
 *              5. _pipeline            → destroyed 8th    (render pass before framebuffers)
 *              6. _swapchain           → destroyed 7th    (framebuffers before render pass + depth view)
 *              7. _syncPool            → destroyed 6th    (semaphores/fences before device)
 *              8. _uniformBuffers      → destroyed 5th    (VMA allocations before VmaAllocator)
 *              9. _descriptorAllocator → destroyed 4th    (pool before uniform buffers are freed)
 *             10. _camera              → destroyed 3rd    (no Vulkan state; order irrelevant)
 *             11. cmd pool / buffers (inline) → destroyed 2nd (managed in destructor body)
 *             12. renderFinishedSemaphores    → destroyed 1st (managed in destructor body)
 *
 *          Thread-safety: not thread-safe. All calls must come from the main thread.
 */
class Renderer {
public:
    /**
     * @brief Initialise the entire Vulkan stack: instance, device, swapchain, pipeline,
     *        uniform buffers, descriptors, camera, sync primitives, and command buffers.
     * @details Also calls glfwSetInputMode to disable the cursor (GLFW_CURSOR_DISABLED)
     *          so raw mouse deltas can be captured without the cursor hitting window edges.
     * @param window Non-owning pointer to the GLFW window created in main(). The window
     *               must outlive this Renderer object (main() guarantees this ordering).
     * @throws RendererInitException or RendererVkException on any failure.
     */
    explicit Renderer(GLFWwindow* window);

    /**
     * @brief Wait for all in-flight GPU work to finish, then destroy all Vulkan objects.
     * @details Calls vkDeviceWaitIdle once — permitted only at shutdown (rule 9).
     *          Command pool, command buffers, and renderFinished semaphores are destroyed
     *          explicitly here before the unique_ptr members are destroyed by C++ member
     *          destruction.
     */
    ~Renderer();

    Renderer(const Renderer&)            = delete; ///< Non-copyable.
    Renderer& operator=(const Renderer&) = delete; ///< Non-copyable.
    Renderer(Renderer&&)                 = delete; ///< Non-movable.
    Renderer& operator=(Renderer&&)      = delete; ///< Non-movable.

    /**
     * @brief Submit and present one frame: update camera → upload UBO →
     *        acquire → record → submit → present.
     * @details Does NOT call vkDeviceWaitIdle. Uses per-frame fences and semaphores
     *          to ensure the CPU and GPU do not step on each other's data.
     *          Called once per event loop iteration from main(). The window pointer is
     *          passed in each call rather than stored as a member — raw C API pointers
     *          must not be stored in class members (CLAUDE.md Rule 1).
     * @param window Raw pointer to the GLFW window; used only to poll input (cursor
     *               position, key states) via GLFW C API calls at the call site.
     *               Not stored. The caller (main()) must ensure the window outlives
     *               this Renderer object.
     * @throws RendererVkException on a fatal Vulkan error during acquisition or presentation.
     */
    void drawFrame(GLFWwindow* window);

private:
    /**
     * @brief Sample GLFW keyboard and cursor state, then update the Camera.
     * @details Called at the top of drawFrame(), before writing the UBO. Computes the
     *          cursor delta (current position minus last stored position) and the frame
     *          delta time (glfwGetTime() minus the last stored timestamp). On the first
     *          call the cursor position is stored but no delta is applied (_firstMouse
     *          guard) to prevent a large initial jump when the cursor moves from its
     *          OS position to the centre of the window.
     * @param window The GLFW window to query for cursor position and key states.
     */
    void updateCamera(GLFWwindow* window);

    /**
     * @brief Write the camera's current view and projection matrices into the UBO for
     *        a given frame-in-flight slot.
     * @details Calls Camera::viewMatrix() and Camera::projMatrix(), assembles a UboMvp,
     *          and calls UniformBuffer::write(). The GPU reads this buffer in the vertex
     *          shader during the draw call submitted in the same frame.
     * @param frameIndex The current frame-in-flight slot index. Selects which
     *                   UniformBuffer and descriptor set to use.
     */
    void uploadUbo(std::size_t frameIndex);

    /**
     * @brief Create a command pool and allocate one command buffer per swapchain image.
     * @details Command buffers are re-recorded every frame. Allocating once and re-recording
     *          is more efficient than allocating and freeing each frame.
     */
    void allocateCommandBuffers();

    /**
     * @brief Record the draw commands for one swapchain image index and frame-in-flight slot.
     * @details Records: begin render pass (clear colour + depth) → set viewport →
     *          set scissor → bind pipeline → bind descriptor set (UBO for this frame slot) →
     *          draw 6 vertices (two world-space triangles) → end render pass.
     * @param imageIndex The swapchain image index (selects which framebuffer to target).
     * @param frameIndex The frame-in-flight slot index (selects which descriptor set to bind).
     */
    void recordCommandBuffer(uint32_t imageIndex, std::size_t frameIndex);

    /// Number of frames to pipeline between CPU recording and GPU execution.
    static constexpr std::size_t kMaxFramesInFlight{2};

    /// Vulkan object members (declaration order = reverse destruction order).
    /// Each member comment explains why it is declared at this position.

    /// Instance + physical device + logical device.
    /// Declared first → destroyed LAST. The VkInstance must outlive every child object.
    std::unique_ptr<DeviceContext>    _device;

    /// VkSurfaceKHR owner. Declared second → destroyed second-to-last (surface before instance).
    std::unique_ptr<WindowContext>    _window;

    /// VmaAllocator owner. Declared after _window → destroyed before _window.
    /// Must be declared before _depth and _uniformBuffers: the VmaAllocator must outlive
    /// every VmaAllocation made through it. C++ reverse-destruction handles this because
    /// _depth and _uniformBuffers are declared after _allocator.
    std::unique_ptr<AllocatorContext> _allocator;

    /// Depth image + GPU memory + VkImageView. Declared after _allocator → destroyed before _allocator.
    /// Declared before _pipeline: Pipeline needs _depth->format() to build the render pass.
    std::unique_ptr<DepthResources>   _depth;

    /// VkDescriptorSetLayout + VkRenderPass + VkPipelineLayout + VkPipeline.
    /// Declared after _depth → destroyed before _depth.
    /// Must outlive SwapchainContext (framebuffers reference the render pass)
    /// and DescriptorAllocator (descriptor sets reference the descriptor set layout).
    std::unique_ptr<Pipeline>         _pipeline;

    /// VkSwapchainKHR + colour views + framebuffers.
    /// Declared after _pipeline → destroyed before _pipeline (framebuffers before render pass).
    std::unique_ptr<SwapchainContext> _swapchain;

    /// Per-frame-in-flight semaphores + fences.
    /// Declared after _swapchain → destroyed before _swapchain.
    std::unique_ptr<FrameSyncPool>    _syncPool;

    /// Per-frame-in-flight UBO buffers. One per slot (kMaxFramesInFlight = 2).
    /// Declared after _syncPool → destroyed before _syncPool.
    /// Declared before _descriptorAllocator because DescriptorAllocator construction
    /// needs the VkBuffer handles from these buffers. C++ constructs in declaration order,
    /// so these are ready when DescriptorAllocator is constructed.
    std::vector<std::unique_ptr<UniformBuffer>> _uniformBuffers;

    /// VkDescriptorPool + per-frame VkDescriptorSets.
    /// Declared after _uniformBuffers → destroyed before _uniformBuffers.
    /// Descriptor sets must be freed (via pool destruction) before the buffers they
    /// reference are destroyed — this ordering achieves that.
    std::unique_ptr<DescriptorAllocator> _descriptorAllocator;

    /// Free-fly camera. Pure GLM math; no Vulkan handles.
    /// Declared after _descriptorAllocator → no destruction-order constraint with Vulkan.
    /// Declared here (not earlier) to avoid any suggestion that it owns Vulkan state.
    std::unique_ptr<Camera>              _camera;

    /// Inline-managed Vulkan handles — destroyed explicitly in ~Renderer() before the
    /// unique_ptr members above are auto-destroyed. They cannot be unique_ptr because
    /// they require the device handle and size information available only at runtime.

    VkCommandPool                _commandPool{VK_NULL_HANDLE}; ///< Pool for all command buffers.
    std::vector<VkCommandBuffer> _commandBuffers;              ///< One per swapchain image.

    /// "Render finished" semaphores, one per swapchain image (NOT per frame-in-flight).
    /// Indexed by the imageIndex returned from vkAcquireNextImageKHR. This semaphore is
    /// signalled by vkQueueSubmit and waited on by vkQueuePresentKHR for the same image.
    /// Must stay associated with the image, not the frame slot — the swapchain can have
    /// more images than frames in flight and reusing a smaller pool would signal a
    /// semaphore still pending on the presentation engine
    /// (VUID-vkQueueSubmit-pSignalSemaphores-00067).
    std::vector<VkSemaphore> _renderFinishedSemaphores;

    /// Per-frame state.

    std::size_t _currentFrame{0}; ///< Cycles [0, kMaxFramesInFlight); selects the active sync/UBO slot.

    /// No raw GLFWwindow* member: C API pointers must not be stored in class members
    /// (Rule 1). The window pointer is passed to drawFrame() by the main loop each
    /// frame and forwarded to updateCamera() at the GLFW C API call sites only.

    double _lastCursorX{0.0}; ///< Previous frame cursor X position (pixels), for delta computation.
    double _lastCursorY{0.0}; ///< Previous frame cursor Y position (pixels), for delta computation.
    double _lastTime{0.0};    ///< Previous frame timestamp in seconds from glfwGetTime().
    bool   _firstMouse{true}; ///< True until the first cursor sample is stored; prevents initial jump.
};
