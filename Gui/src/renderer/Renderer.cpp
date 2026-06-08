/**
 * @file renderer/Renderer.cpp
 * @brief Implementation of Renderer: construction, drawFrame(), and teardown.
 * @details The Renderer constructor must follow a strict ordering because each Vulkan
 *          object depends on handles from the objects created before it:
 *
 *            DeviceContext::createInstance()      → VkInstance
 *            WindowContext(window, instance)       → VkSurfaceKHR
 *            DeviceContext(instance, surface)      → VkDevice + VkQueue + VkPhysicalDevice
 *            [query surface format]                → VkFormat (for Pipeline)
 *            Pipeline(device, format, shadersDir)  → VkRenderPass + VkPipeline
 *            SwapchainContext(device, phys, surf,  → VkSwapchainKHR + views + framebuffers
 *                             renderPass, w, h)
 *            FrameSyncPool(device, 2)              → 4× VkSemaphore + 2× VkFence
 *            allocateCommandBuffers()              → VkCommandPool + N× VkCommandBuffer
 *
 *          The per-frame loop in drawFrame() follows the Vulkan present pipeline:
 *            1. vkWaitForFences — block CPU until previous use of this frame slot is done
 *            2. vkAcquireNextImageKHR — ask the swapchain for the next presentable image
 *            3. vkResetFences — un-signal the fence so we can wait on it next frame
 *            4. recordCommandBuffer — re-record draw commands for the acquired image
 *            5. vkQueueSubmit — submit commands to the GPU
 *            6. vkQueuePresentKHR — ask the swapchain to present the rendered image
 *
 *          Architecture: Renderer.cpp is the only translation unit that includes all four
 *          sub-object headers. Keeping all the wiring here prevents circular dependencies
 *          and makes the initialisation sequence easy to audit.
 */

#include "renderer/Renderer.hpp"
#include "renderer/device/VkCheck.hpp"
#include "exceptions.hpp"

#include <spdlog/spdlog.h>

// ─── constructor ──────────────────────────────────────────────────────────────

/**
 * @brief Initialise the entire Vulkan stack.
 * @param window Non-owning pointer to the GLFW window created in main().
 */
Renderer::Renderer(GLFWwindow* window)
{
    // ── Step 1: Create VkInstance ─────────────────────────────────────────────
    //
    // createInstance() is static — it does not need a DeviceContext object yet.
    // The instance is needed to create the surface, and the surface is needed to
    // pick the physical device, so the instance must come first.
    VkInstance instance = DeviceContext::createInstance();

    // ── Step 2: Create the window surface ────────────────────────────────────
    //
    // WindowContext receives the GLFWwindow* at the call site and creates the VkSurfaceKHR.
    // The surface is needed immediately to inform device selection (not all GPUs
    // support presentation on all surfaces).
    _window = std::make_unique<WindowContext>(window, instance);

    // ── Step 3: Create the logical device ────────────────────────────────────
    //
    // DeviceContext uses the surface to verify that the chosen GPU supports
    // presentation. It selects the best GPU and creates VkDevice + VkQueue.
    _device = std::make_unique<DeviceContext>(instance, _window->surface());

    // ── Step 4: Query the surface format for Pipeline creation ───────────────
    //
    // Pipeline needs the swapchain image format to configure the render pass
    // colour attachment, but the swapchain does not exist yet (it needs the
    // render pass handle to create framebuffers — a chicken-and-egg situation).
    //
    // Resolution: query the surface formats directly from the physical device
    // and pick the same format that SwapchainContext will choose (B8G8R8A8_SRGB,
    // falling back to the first available format). This mirrors vk-bootstrap's
    // internal logic so the two formats always agree.
    VkFormat surfaceFormat = VK_FORMAT_B8G8R8A8_SRGB; // preferred

    {
        // vkGetPhysicalDeviceSurfaceFormatsKHR: query which (format, color-space)
        // combinations the GPU + surface combination supports.
        uint32_t formatCount = 0;
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
            _device->physicalDevice(), _window->surface(), &formatCount, nullptr));

        if (formatCount == 0) {
            throw RendererInitException("vkGetPhysicalDeviceSurfaceFormatsKHR: no formats");
        }

        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
            _device->physicalDevice(), _window->surface(), &formatCount, formats.data()));

        // Check whether the preferred format is available.
        bool found = false;
        for (const VkSurfaceFormatKHR& fmt : formats) {
            if (fmt.format == VK_FORMAT_B8G8R8A8_SRGB &&
                fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                found = true;
                break;
            }
        }

        // Fall back to the first available format if our preference is unavailable.
        if (!found) {
            surfaceFormat = formats[0].format;
            spdlog::warn("Renderer: preferred surface format unavailable, "
                         "falling back to format {}",
                         static_cast<int>(surfaceFormat));
        }
    }

    // ── Step 5: Create the graphics pipeline ─────────────────────────────────
    //
    // Pipeline loads the compiled SPIR-V shaders, creates the render pass (using
    // the surface format queried above), and assembles the graphics pipeline.
    // The render pass handle is then used in Step 6 for framebuffer creation.
    _pipeline = std::make_unique<Pipeline>(
        _device->device(),
        surfaceFormat,
        SHADERS_DIR);

    // ── Step 6: Create the swapchain, image views, and framebuffers ───────────
    //
    // SwapchainContext creates the VkSwapchainKHR (the ring of presentable images),
    // one VkImageView per image, and one VkFramebuffer per view. The framebuffers
    // reference the render pass from Step 5.
    uint32_t fbWidth{0};
    uint32_t fbHeight{0};
    // Pass the window pointer at the call site — WindowContext does not store it.
    _window->framebufferSize(window, fbWidth, fbHeight);

    _swapchain = std::make_unique<SwapchainContext>(
        _device->device(),
        _device->physicalDevice(),
        _window->surface(),
        _pipeline->renderPass(),
        fbWidth,
        fbHeight);

    // ── Step 7: Create per-frame synchronisation primitives ──────────────────
    //
    // FrameSyncPool allocates 2 × imageAvailable semaphores, 2 × renderFinished
    // semaphores, and 2 × inFlight fences (one set per frame-in-flight slot).
    _syncPool = std::make_unique<FrameSyncPool>(_device->device(), kMaxFramesInFlight);

    // ── Step 8: Allocate command pool and command buffers ─────────────────────
    allocateCommandBuffers();

    spdlog::info("Renderer: initialisation complete.");
}

// ─── destructor ───────────────────────────────────────────────────────────────

/**
 * @brief Wait for GPU to finish all in-flight work, then destroy all Vulkan objects.
 */
Renderer::~Renderer()
{
    // vkDeviceWaitIdle: block the CPU until all submitted GPU work completes.
    // Permitted here (destructor = shutdown, not the hot path — rule 9).
    // This is required before destroying any Vulkan object that might still be
    // referenced by in-flight GPU commands.
    if (_device && _device->device() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(_device->device());
        spdlog::debug("Renderer: vkDeviceWaitIdle completed.");
    }

    // Destroy command pool (and implicitly all command buffers allocated from it).
    // Must happen before _syncPool, _swapchain, _pipeline, _window, _device
    // are destroyed by C++ member destruction below.
    if (_commandPool != VK_NULL_HANDLE && _device) {
        // vkDestroyCommandPool: frees all command buffers allocated from this pool
        // and then frees the pool itself. No need to call vkFreeCommandBuffers first.
        vkDestroyCommandPool(_device->device(), _commandPool, nullptr);
        _commandPool = VK_NULL_HANDLE;
        spdlog::debug("Renderer: VkCommandPool destroyed.");
    }

    // unique_ptr members are now destroyed in reverse declaration order:
    //   _syncPool  → vkDestroySemaphore × 4 + vkDestroyFence × 2
    //   _swapchain → vkDestroyFramebuffer × N + vkDestroyImageView × N + vkDestroySwapchainKHR
    //   _pipeline  → vkDestroyPipeline + vkDestroyPipelineLayout + vkDestroyRenderPass
    //   _window    → vkDestroySurfaceKHR
    //   _device    → vkDestroyDevice + (debug messenger) + vkDestroyInstance
}

// ─── private helpers ──────────────────────────────────────────────────────────

/**
 * @brief Create a command pool and allocate one command buffer per swapchain image.
 */
void Renderer::allocateCommandBuffers()
{
    // ── Command pool creation ─────────────────────────────────────────────────
    //
    // VkCommandPool: a memory pool from which command buffers are allocated.
    // Creating and destroying individual command buffers is expensive; pooling
    // allows efficient reuse. All buffers in a pool share the same queue family.
    //
    // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT: allows individual command
    // buffers to be reset and re-recorded via vkResetCommandBuffer, without
    // resetting the entire pool. Required because we re-record per frame.
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = _device->graphicsQueueFamily();
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VK_CHECK(vkCreateCommandPool(_device->device(), &poolInfo, nullptr, &_commandPool));
    spdlog::debug("Renderer: VkCommandPool created.");

    // ── Command buffer allocation ─────────────────────────────────────────────
    //
    // VkCommandBuffer: a recorded list of GPU commands (draw calls, pipeline binds,
    // render pass boundaries). The CPU records it; the GPU executes it when submitted
    // to a queue. One buffer per swapchain image so we can record frame N+1 while the
    // GPU executes frame N.
    //
    // VK_COMMAND_BUFFER_LEVEL_PRIMARY: can be submitted directly to a queue (as opposed
    // to SECONDARY which must be called from a primary buffer via vkCmdExecuteCommands).
    _commandBuffers.resize(_swapchain->imageCount());

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = _commandPool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(_commandBuffers.size());

    VK_CHECK(vkAllocateCommandBuffers(
        _device->device(), &allocInfo, _commandBuffers.data()));

    spdlog::debug("Renderer: {} command buffers allocated.", _commandBuffers.size());
}

/**
 * @brief Record the draw commands for one swapchain image index.
 * @param imageIndex Index of the swapchain image whose framebuffer to target.
 */
void Renderer::recordCommandBuffer(uint32_t imageIndex)
{
    VkCommandBuffer cmd = _commandBuffers[imageIndex];

    // vkResetCommandBuffer: discard the previously recorded commands in this buffer
    // so we can record new ones. The buffer returns to the initial state.
    // VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT: free the memory used by the
    // previous recording, not just mark it available for reuse (avoids fragmentation
    // over many frames).
    VK_CHECK(vkResetCommandBuffer(cmd, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT));

    // ── Begin command buffer ──────────────────────────────────────────────────
    //
    // vkBeginCommandBuffer: transition the buffer from Initial to Recording state.
    // ONE_TIME_SUBMIT_BIT would hint that the buffer is submitted once then reset —
    // we do not set it because we re-use these buffers repeatedly each frame.
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;

    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    // ── Begin render pass ─────────────────────────────────────────────────────
    //
    // vkCmdBeginRenderPass: start the render pass, binding the framebuffer and
    // clearing the colour attachment with the specified clear colour.
    // Physically: on tile-based GPUs (mobile), this signals the start of a tile
    // render. On desktop GPUs it primarily performs the layout transition from
    // UNDEFINED to COLOR_ATTACHMENT_OPTIMAL and issues the clear operation.
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass        = _pipeline->renderPass();
    renderPassInfo.framebuffer       = _swapchain->framebuffer(imageIndex);
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {_swapchain->width(), _swapchain->height()};

    // Clear colour: dark grey background so the RGB triangle is clearly visible.
    VkClearValue clearColor{};
    clearColor.color = {{0.1f, 0.1f, 0.1f, 1.0f}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues    = &clearColor;

    // VK_SUBPASS_CONTENTS_INLINE: all draw commands are recorded inline in this
    // primary command buffer (as opposed to SECONDARY_COMMAND_BUFFERS mode where
    // secondary buffers are executed via vkCmdExecuteCommands).
    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // ── Bind graphics pipeline ────────────────────────────────────────────────
    //
    // vkCmdBindPipeline: set the pipeline that subsequent draw commands will use.
    // GRAPHICS: this is a graphics pipeline (as opposed to COMPUTE).
    // Physically: the GPU loads the compiled shader programs and the fixed-function
    // state (rasteriser settings, blend state, etc.) for the draw calls that follow.
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline->pipeline());

    // ── Set dynamic viewport ──────────────────────────────────────────────────
    //
    // vkCmdSetViewport: define the viewport transform — maps NDC [-1,1] to pixel space.
    // x=0, y=0: top-left of the framebuffer.
    // width = swapchain width, height = swapchain height: full framebuffer coverage.
    // minDepth=0, maxDepth=1: standard depth range.
    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(_swapchain->width());
    viewport.height   = static_cast<float>(_swapchain->height());
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    // ── Set dynamic scissor ───────────────────────────────────────────────────
    //
    // vkCmdSetScissor: define the scissor rectangle — fragments outside are discarded.
    // Full framebuffer coverage: no scissoring.
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {_swapchain->width(), _swapchain->height()};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // ── Draw ──────────────────────────────────────────────────────────────────
    //
    // vkCmdDraw: record a non-indexed draw call.
    // vertexCount=3: the vertex shader runs 3 times, once per vertex.
    //               gl_VertexIndex in the shader is 0, 1, 2 on the three invocations.
    // instanceCount=1: one instance (no instanced rendering).
    // firstVertex=0: gl_VertexIndex starts at 0.
    // firstInstance=0: gl_InstanceIndex starts at 0.
    // The GPU assembles the 3 vertices into one triangle (TRIANGLE_LIST topology)
    // and rasterises it — producing one fragment per covered pixel. The fragment
    // shader runs once per fragment, outputting the interpolated colour.
    vkCmdDraw(cmd, 3, 1, 0, 0);

    // ── End render pass ───────────────────────────────────────────────────────
    //
    // vkCmdEndRenderPass: finalise the render pass.
    // Physically: on tile-based GPUs this flushes the tile data to main memory.
    // On desktop GPUs it primarily performs the layout transition from
    // COLOR_ATTACHMENT_OPTIMAL to PRESENT_SRC_KHR (as declared in the render pass).
    vkCmdEndRenderPass(cmd);

    // ── End command buffer ────────────────────────────────────────────────────
    //
    // vkEndCommandBuffer: transition from Recording to Executable state.
    // The buffer is now ready to be submitted to a queue.
    VK_CHECK(vkEndCommandBuffer(cmd));
}

// ─── drawFrame ────────────────────────────────────────────────────────────────

/**
 * @brief Submit and present one frame.
 */
void Renderer::drawFrame()
{
    FrameSync& sync = (*_syncPool)[_currentFrame];

    // ── Wait for previous use of this frame slot ──────────────────────────────
    //
    // vkWaitForFences: block the CPU until the GPU signals the inFlight fence.
    // This ensures the command buffer for this slot has finished executing on the GPU
    // before we re-record it. Without this wait, the CPU would overwrite commands
    // the GPU is still reading — corrupted rendering or a GPU hang.
    //
    // VK_TRUE: wait for ALL listed fences (we have only one).
    // UINT64_MAX: no timeout (wait forever). Acceptable here because validation layers
    //             will catch infinite waits caused by programming errors.
    VK_CHECK(vkWaitForFences(
        _device->device(), 1, &sync.inFlight, VK_TRUE, UINT64_MAX));

    // ── Acquire the next swapchain image ──────────────────────────────────────
    //
    // vkAcquireNextImageKHR: ask the swapchain for the index of the next image
    // that is free for rendering. The GPU signals imageAvailable when the image
    // is truly ready (not being scanned out by the display hardware).
    //
    // The returned imageIndex may not equal _currentFrame — the swapchain
    // manages its own image ordering, which may differ from our frame-in-flight
    // index. We use imageIndex to select the framebuffer; _currentFrame to select
    // the sync objects.
    uint32_t imageIndex{0};
    VK_CHECK(vkAcquireNextImageKHR(
        _device->device(),
        _swapchain->swapchain(),
        UINT64_MAX,
        sync.imageAvailable,
        VK_NULL_HANDLE,
        &imageIndex));

    // ── Reset the fence before re-submitting ─────────────────────────────────
    //
    // vkResetFences: return the fence to the unsignalled state so vkQueueSubmit
    // can signal it again when this frame's commands complete.
    // Reset AFTER acquiring the image (not before) so the fence is only reset when
    // we are sure we will submit work this frame.
    VK_CHECK(vkResetFences(_device->device(), 1, &sync.inFlight));

    // ── Record the command buffer for this image ──────────────────────────────
    recordCommandBuffer(imageIndex);

    // ── Submit the command buffer ─────────────────────────────────────────────
    //
    // vkQueueSubmit: hand the recorded command buffer to the GPU for execution.
    // The submission specifies:
    //   waitSemaphores:   imageAvailable — the GPU must wait until the swapchain
    //                     image is ready before executing colour output commands.
    //   signalSemaphores: renderFinished — the GPU signals this when done, allowing
    //                     vkQueuePresentKHR to proceed.
    //   signalFences:     inFlight — the GPU signals this when the submission
    //                     completes, allowing the CPU to reuse this frame's resources.
    //
    // waitDstStageMask: the stage at which the wait applies. COLOR_ATTACHMENT_OUTPUT
    // means the GPU is allowed to proceed up to (but not including) writing to the
    // colour attachment until imageAvailable is signalled. This allows the GPU to
    // run vertex shaders etc. while still waiting for the image.
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo{};
    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount   = 1;
    submitInfo.pWaitSemaphores      = &sync.imageAvailable;
    submitInfo.pWaitDstStageMask    = &waitStage;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &_commandBuffers[imageIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = &sync.renderFinished;

    VK_CHECK(vkQueueSubmit(_device->graphicsQueue(), 1, &submitInfo, sync.inFlight));

    // ── Present the rendered image ────────────────────────────────────────────
    //
    // vkQueuePresentKHR: ask the swapchain to display the rendered image.
    // waitSemaphores: renderFinished — the swapchain waits until the GPU has finished
    //                 rendering before swapping the image to the display.
    // Physically: the display hardware starts scanning out the new image on the next
    // vertical blank (vsync, because we selected VK_PRESENT_MODE_FIFO_KHR).
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &sync.renderFinished;
    presentInfo.swapchainCount     = 1;
    const VkSwapchainKHR sc        = _swapchain->swapchain();
    presentInfo.pSwapchains        = &sc;
    presentInfo.pImageIndices      = &imageIndex;

    VK_CHECK(vkQueuePresentKHR(_device->graphicsQueue(), &presentInfo));

    // Advance to the next frame-in-flight slot.
    _currentFrame = (_currentFrame + 1) % kMaxFramesInFlight;
}
