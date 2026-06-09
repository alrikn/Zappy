/**
 * @file renderer/Renderer.cpp
 * @brief Implementation of Renderer: construction, camera update, UBO upload, draw loop, teardown.
 * @details The Renderer constructor must follow a strict ordering because each Vulkan
 *          object depends on handles from the objects created before it:
 *
 *            DeviceContext::createInstance()      → VkInstance
 *            WindowContext(window, instance)       → VkSurfaceKHR
 *            DeviceContext(instance, surface)      → VkDevice + VkQueue + VkPhysicalDevice
 *            AllocatorContext(instance, phys, dev) → VmaAllocator
 *            [query framebuffer size]              → (fbWidth, fbHeight)
 *            DepthResources(allocator, dev, phys,  → VkImage + VmaAllocation + VkImageView
 *                           fbWidth, fbHeight)        + VkFormat (probed)
 *            [query surface format]                → VkFormat (for Pipeline colour attachment)
 *            Pipeline(device, colorFormat,         → VkDescriptorSetLayout + VkRenderPass
 *                     depthFormat, shadersDir)          + VkPipelineLayout + VkPipeline
 *            SwapchainContext(device, phys, surf,  → VkSwapchainKHR + colour views
 *                             renderPass,              + framebuffers (colour + depth view)
 *                             depthView, w, h)
 *            FrameSyncPool(device, 2)              → 2× VkSemaphore + 2× VkFence
 *            UniformBuffer × kMaxFramesInFlight    → 2× host-coherent VMA buffer (persistently mapped)
 *            DescriptorAllocator(device, layout,   → VkDescriptorPool + 2× VkDescriptorSet
 *                                uniformBuffers)       (each set bound to one UniformBuffer)
 *            Camera(aspectRatio)                   → pure-math camera, no Vulkan objects
 *            renderFinished semaphores             → N× VkSemaphore (one per swapchain image)
 *            allocateCommandBuffers()              → VkCommandPool + N× VkCommandBuffer
 *
 *          The per-frame loop in drawFrame() follows this sequence:
 *            1. updateCamera()          — poll GLFW input, update yaw/pitch/position
 *            2. uploadUbo()             — write view+proj matrices to the UBO for this frame slot
 *            3. vkWaitForFences         — block CPU until previous use of this frame slot is done
 *            4. vkAcquireNextImageKHR   — ask the swapchain for the next presentable image
 *            5. vkResetFences           — un-signal the fence so we can wait on it next frame
 *            6. recordCommandBuffer()   — re-record draw commands including descriptor set bind
 *            7. vkQueueSubmit           — submit commands to the GPU
 *            8. vkQueuePresentKHR       — ask the swapchain to present the rendered image
 *
 *          Architecture: Renderer.cpp is the only translation unit that includes all
 *          sub-object headers. Keeping all the wiring here prevents circular dependencies
 *          and makes the initialisation sequence easy to audit.
 */

#include "renderer/Renderer.hpp"
#include "renderer/device/VkCheck.hpp"
#include "renderer/scene/UboMvp.hpp"
#include "exceptions.hpp"

#include <array>
#include <spdlog/spdlog.h>

// ─── constructor ──────────────────────────────────────────────────────────────

/**
 * @brief Initialise the entire Vulkan stack including UBO, descriptors, and camera.
 * @param window Non-owning pointer to the GLFW window created in main(). Used only
 *               at C API call sites in the constructor body — never stored as a member.
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

    // ── Step 4: Create the VMA allocator ─────────────────────────────────────
    //
    // AllocatorContext wraps vmaCreateAllocator. VMA needs the Vulkan triple
    // (instance + physical device + device) to allocate and manage GPU memory.
    // Created here, after all three handles exist, and before DepthResources
    // which is the first class to allocate through VMA.
    _allocator = std::make_unique<AllocatorContext>(
        _device->instance(),
        _device->physicalDevice(),
        _device->device());

    // ── Step 5: Create the depth image ───────────────────────────────────────
    //
    // DepthResources probes the GPU for a supported depth format, allocates a
    // depth image through VMA, and creates its VkImageView.
    //
    // We query the framebuffer size here (before Pipeline) because DepthResources
    // needs the exact pixel dimensions to size the depth image to match the
    // swapchain images. Querying early also avoids a second glfwGetFramebufferSize
    // call later — we can reuse fbWidth/fbHeight for SwapchainContext as well.
    uint32_t fbWidth{0};
    uint32_t fbHeight{0};
    // Pass the window pointer at the call site — WindowContext does not store it.
    _window->framebufferSize(window, fbWidth, fbHeight);

    _depth = std::make_unique<DepthResources>(
        _allocator->allocator(),
        _device->device(),
        _device->physicalDevice(),
        fbWidth,
        fbHeight);

    // ── Step 6: Query the surface format for Pipeline creation ───────────────
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

    // ── Step 7: Create the graphics pipeline ─────────────────────────────────
    //
    // Pipeline loads the compiled SPIR-V shaders, creates the VkDescriptorSetLayout
    // for binding 0 (the UBO), creates the render pass with both colour and depth
    // attachments, creates the pipeline layout referencing the descriptor set layout,
    // and assembles the full graphics pipeline with depth test enabled.
    _pipeline = std::make_unique<Pipeline>(
        _device->device(),
        surfaceFormat,
        _depth->format(),
        SHADERS_DIR);

    // ── Step 8: Create the swapchain, colour views, and framebuffers ──────────
    //
    // SwapchainContext creates the VkSwapchainKHR (the ring of presentable images),
    // one colour VkImageView per image, and one VkFramebuffer per view. Each
    // framebuffer has two attachments: the per-image colour view and the shared
    // depth view. The depth view is borrowed from DepthResources and must outlive
    // SwapchainContext (guaranteed by declaration order in Renderer.hpp).
    _swapchain = std::make_unique<SwapchainContext>(
        _device->device(),
        _device->physicalDevice(),
        _window->surface(),
        _pipeline->renderPass(),
        _depth->view(),
        fbWidth,
        fbHeight);

    // ── Step 9: Create per-frame synchronisation primitives ──────────────────
    //
    // FrameSyncPool allocates 2 × imageAvailable semaphores and 2 × inFlight fences
    // (one set per frame-in-flight slot).
    _syncPool = std::make_unique<FrameSyncPool>(_device->device(), kMaxFramesInFlight);

    // ── Step 10: Create one UniformBuffer per frame-in-flight slot ───────────
    //
    // Each frame-in-flight slot has its own UBO buffer so the CPU can write the
    // next frame's camera matrices while the GPU is still reading the previous
    // frame's matrices from a different buffer. Without this double-buffering, the
    // CPU write would race with the GPU read — corrupted camera data.
    //
    // The UniformBuffer objects are stored as unique_ptr in a vector so they can
    // be constructed one by one while the vector is built, without requiring all
    // buffers to be created atomically (which would complicate error handling).
    _uniformBuffers.reserve(kMaxFramesInFlight);
    for (std::size_t i = 0; i < kMaxFramesInFlight; ++i) {
        _uniformBuffers.push_back(
            std::make_unique<UniformBuffer>(_allocator->allocator()));
    }
    spdlog::debug("Renderer: {} UniformBuffers created.", kMaxFramesInFlight);

    // ── Step 11: Create the DescriptorAllocator ───────────────────────────────
    //
    // DescriptorAllocator creates a VkDescriptorPool large enough for kMaxFramesInFlight
    // uniform-buffer descriptors, allocates one VkDescriptorSet per frame slot, and
    // immediately updates each set to point at the corresponding UniformBuffer.
    //
    // We pass raw UniformBuffer* pointers here. DescriptorAllocator only uses them
    // during construction (to fill VkDescriptorBufferInfo) and does not store them.
    // The vector of raw pointers is built on the stack and discarded after construction.
    {
        std::vector<UniformBuffer*> uboPtrs;
        uboPtrs.reserve(kMaxFramesInFlight);
        for (const auto& ubo : _uniformBuffers) {
            uboPtrs.push_back(ubo.get());
        }
        _descriptorAllocator = std::make_unique<DescriptorAllocator>(
            _device->device(),
            _pipeline->descriptorSetLayout(),
            uboPtrs);
    }

    // ── Step 12: Create the Camera ────────────────────────────────────────────
    //
    // The camera is a pure-math object (no Vulkan state). It is constructed after
    // the swapchain so the correct aspect ratio (swapchain width / height) can be
    // passed in. The projection matrix is computed once and cached.
    const float aspectRatio = static_cast<float>(fbWidth) / static_cast<float>(fbHeight);
    _camera = std::make_unique<Camera>(aspectRatio);

    // ── Step 13: Capture the mouse cursor ────────────────────────────────────
    //
    // GLFW_CURSOR_DISABLED: hide the cursor and lock it to the window so the OS
    // does not clip cursor movement at window edges. GLFW will continue to report
    // raw cursor delta even when the cursor "moves" past the window boundary.
    // Without this, mouse-look would stop working when the cursor hits an edge.
    //
    // The OS restores the cursor automatically when the window loses focus.
    // No escape-key toggle is implemented in this feature — the window can be closed
    // with Alt+F4 or by the window manager's close button.
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // Initialise the last-time timestamp so the first frame delta is ~0 rather than
    // the full elapsed time since the process started.
    _lastTime = glfwGetTime();

    // ── Step 14: Create "render finished" semaphores ──────────────────────────
    //
    // One semaphore per swapchain image (NOT per frame-in-flight slot).
    // Indexed by the imageIndex returned from vkAcquireNextImageKHR. This semaphore
    // is signalled by vkQueueSubmit and waited on by vkQueuePresentKHR for the SAME
    // swapchain image, so it must be indexed by swapchain image — the swapchain can
    // have more images than frames in flight, and reusing a smaller pool would signal
    // a semaphore still pending on the presentation engine
    // (VUID-vkQueueSubmit-pSignalSemaphores-00067).
    {
        VkSemaphoreCreateInfo semInfo{};
        semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        _renderFinishedSemaphores.resize(_swapchain->imageCount());
        for (VkSemaphore& sem : _renderFinishedSemaphores) {
            VK_CHECK(vkCreateSemaphore(_device->device(), &semInfo, nullptr, &sem));
        }
    }

    // ── Step 15: Allocate command pool and command buffers ────────────────────
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
    // referenced by in-flight GPU commands (command buffers, semaphores, UBO buffers).
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

    // Destroy the per-swapchain-image "render finished" semaphores. Same rationale
    // as _commandPool: raw handles owned directly by Renderer, must be destroyed
    // here while _device is still valid.
    if (_device) {
        for (VkSemaphore sem : _renderFinishedSemaphores) {
            if (sem != VK_NULL_HANDLE) {
                vkDestroySemaphore(_device->device(), sem, nullptr);
            }
        }
        _renderFinishedSemaphores.clear();
        spdlog::debug("Renderer: per-image renderFinished semaphores destroyed.");
    }

    // unique_ptr members are now destroyed in reverse declaration order:
    //   _camera              → ~Camera() (no Vulkan calls)
    //   _descriptorAllocator → vkDestroyDescriptorPool (frees all sets)
    //   _uniformBuffers      → vmaDestroyBuffer × 2
    //   _syncPool            → vkDestroySemaphore × 2 + vkDestroyFence × 2
    //   _swapchain           → vkDestroyFramebuffer × N + vkDestroyImageView × N + vkDestroySwapchainKHR
    //   _pipeline            → vkDestroyPipeline + vkDestroyPipelineLayout + vkDestroyDescriptorSetLayout + vkDestroyRenderPass
    //   _depth               → vkDestroyImageView + vmaDestroyImage (image + allocation)
    //   _allocator           → vmaDestroyAllocator
    //   _window              → vkDestroySurfaceKHR
    //   _device              → vkDestroyDevice + (debug messenger) + vkDestroyInstance
}

// ─── private helpers ──────────────────────────────────────────────────────────

/**
 * @brief Sample GLFW input and update the Camera for this frame.
 * @param window The GLFW window to query.
 */
void Renderer::updateCamera(GLFWwindow* window)
{
    // ── Cursor delta computation ──────────────────────────────────────────────
    //
    // glfwGetCursorPos: returns the current cursor position in screen pixels.
    // When GLFW_CURSOR_DISABLED is active, the cursor is hidden and locked to the
    // window centre; GLFW reports an ever-accumulating position rather than one
    // clamped to the window bounds — giving us true raw mouse motion.
    double cursorX{0.0};
    double cursorY{0.0};
    glfwGetCursorPos(window, &cursorX, &cursorY);

    if (_firstMouse) {
        // First frame: store the initial position but do NOT compute a delta.
        // Without this guard, the delta would be (cursorX - 0.0, cursorY - 0.0),
        // which could be hundreds of pixels if the OS cursor was positioned away
        // from the window origin — causing the camera to snap to a random direction.
        _lastCursorX = cursorX;
        _lastCursorY = cursorY;
        _firstMouse  = false;
    }

    // Compute the pixel delta since the last frame.
    const float dx = static_cast<float>(cursorX - _lastCursorX);
    const float dy = static_cast<float>(cursorY - _lastCursorY);
    _lastCursorX = cursorX;
    _lastCursorY = cursorY;

    // Apply the delta to the camera's yaw and pitch.
    _camera->processMouseDelta(dx, dy);

    // ── Frame delta time ──────────────────────────────────────────────────────
    //
    // glfwGetTime(): returns elapsed seconds since glfwInit() was called.
    // The difference gives the duration of the last frame in seconds, which is
    // multiplied by kSpeed inside Camera::processKeyboard() to give frame-rate-
    // independent movement.
    const double currentTime = glfwGetTime();
    const float  dt          = static_cast<float>(currentTime - _lastTime);
    _lastTime                = currentTime;

    // ── Keyboard movement ─────────────────────────────────────────────────────
    //
    // glfwGetKey: returns GLFW_PRESS if the key is currently held down.
    // We read the four movement keys and pass booleans to the camera.
    const bool forward  = (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS);
    const bool backward = (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS);
    const bool left     = (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS);
    const bool right    = (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS);

    _camera->processKeyboard(forward, backward, left, right, dt);
}

/**
 * @brief Write the camera's view + proj matrices into the UBO for the given frame slot.
 * @param frameIndex The current frame-in-flight slot index.
 */
void Renderer::uploadUbo(std::size_t frameIndex)
{
    // Assemble the UboMvp struct from the current camera state.
    // viewMatrix() and projMatrix() are const — they do not modify the camera.
    UboMvp ubo{};
    ubo.view = _camera->viewMatrix();
    ubo.proj = _camera->projMatrix();

    // Write the 128-byte struct into the persistently-mapped VMA buffer for this slot.
    // The HOST_COHERENT flag ensures the GPU sees the updated bytes without a flush call.
    // This call happens before vkQueueSubmit, guaranteeing the GPU reads the new values
    // during the draw commands submitted in the same frame.
    _uniformBuffers[frameIndex]->write(ubo);
}

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
 * @brief Record the draw commands for one swapchain image index and frame-in-flight slot.
 * @param imageIndex The swapchain image index (selects which framebuffer to target).
 * @param frameIndex The frame-in-flight slot index (selects which descriptor set to bind).
 */
void Renderer::recordCommandBuffer(uint32_t imageIndex, std::size_t frameIndex)
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
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;

    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    // ── Begin render pass ─────────────────────────────────────────────────────
    //
    // vkCmdBeginRenderPass: start the render pass, binding the framebuffer and
    // clearing the attachments with the specified clear values.
    // Physically: on tile-based GPUs (mobile), this signals the start of a tile
    // render. On desktop GPUs it performs the layout transition and clears.
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass        = _pipeline->renderPass();
    renderPassInfo.framebuffer       = _swapchain->framebuffer(imageIndex);
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {_swapchain->width(), _swapchain->height()};

    // Clear values for both render pass attachments (indexed by attachment slot).
    //   clearValues[0] → attachment 0 (colour): dark grey background.
    //   clearValues[1] → attachment 1 (depth):  1.0 = far-plane value (cleared every frame).
    //
    // Depth clear value 1.0f: with VK_COMPARE_OP_LESS, any fragment at depth < 1.0 passes
    // the test on the first draw. Clearing to 0.0 instead would cause all fragments to fail.
    const std::array<VkClearValue, 2> clearValues = []{
        std::array<VkClearValue, 2> cv{};
        cv[0].color        = {{0.1f, 0.1f, 0.1f, 1.0f}};
        cv[1].depthStencil = {1.0f, 0};
        return cv;
    }();
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues    = clearValues.data();

    // VK_SUBPASS_CONTENTS_INLINE: all draw commands are recorded inline in this
    // primary command buffer (no secondary command buffer execution).
    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // ── Bind graphics pipeline ────────────────────────────────────────────────
    //
    // vkCmdBindPipeline: set the pipeline that subsequent draw commands will use.
    // GRAPHICS: this is a graphics pipeline (as opposed to COMPUTE).
    // Physically: the GPU loads the compiled shader programs and fixed-function state.
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline->pipeline());

    // ── Set dynamic viewport ──────────────────────────────────────────────────
    //
    // vkCmdSetViewport: define the viewport transform — maps NDC [-1,1] to pixel space.
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
    // vkCmdSetScissor: fragments outside this rectangle are discarded. Full framebuffer.
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {_swapchain->width(), _swapchain->height()};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // ── Bind the UBO descriptor set ───────────────────────────────────────────
    //
    // vkCmdBindDescriptorSets: tell the GPU which descriptor sets to use for the
    // next draw calls. The descriptor set at set=0 binding=0 points to the
    // UniformBuffer for this frame slot, which holds the view+proj matrices written
    // by uploadUbo() earlier in this frame.
    //
    // GRAPHICS: the descriptor set is bound for the graphics pipeline (not compute).
    // pipelineLayout: the layout that declares the descriptor set structure — must
    //   match the layout used to create the pipeline and to allocate the sets.
    // firstSet=0: binding starts at set index 0.
    // descriptorSetCount=1: we bind exactly one set (set=0).
    // pDescriptorSets: pointer to the set for this frame-in-flight slot.
    // dynamicOffsetCount=0: no dynamic offsets (our UBO occupies the full buffer).
    //
    // Physically: the GPU records the descriptor set binding in the command buffer.
    // When the draw call executes, the vertex shader reads the set's binding 0 to
    // find the UboMvp buffer and fetches the view+proj matrices from it.
    //
    // Skipping this bind: the vertex shader would read from whatever descriptor set
    // was previously bound (undefined at the first frame) — invalid data, GPU crash,
    // or validation error VUID-vkCmdDraw-None-02700.
    VkDescriptorSet descSet = _descriptorAllocator->set(frameIndex);
    vkCmdBindDescriptorSets(
        cmd,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        _pipeline->pipelineLayout(),
        0,       // firstSet
        1,       // descriptorSetCount
        &descSet,
        0,       // dynamicOffsetCount
        nullptr  // pDynamicOffsets
    );

    // ── Draw ──────────────────────────────────────────────────────────────────
    //
    // vkCmdDraw: record a non-indexed draw call.
    // vertexCount=6: the vertex shader runs 6 times.
    //   gl_VertexIndex 0–2 → Triangle A (RGB gradient).
    //   gl_VertexIndex 3–5 → Triangle B (solid cyan).
    // instanceCount=1, firstVertex=0, firstInstance=0: standard single draw.
    //
    // Each vertex shader invocation reads the UboMvp from the bound descriptor
    // set (binding 0) and computes: gl_Position = proj * view * vec4(pos, 1.0).
    // The depth buffer is cleared to 1.0 at render pass begin; perspective-divide
    // produces natural depth ordering so the depth test resolves occlusion correctly.
    vkCmdDraw(cmd, 6, 1, 0, 0);

    // ── End render pass ───────────────────────────────────────────────────────
    //
    // vkCmdEndRenderPass: finalise the render pass, triggering layout transitions.
    vkCmdEndRenderPass(cmd);

    // ── End command buffer ────────────────────────────────────────────────────
    //
    // vkEndCommandBuffer: transition from Recording to Executable state.
    VK_CHECK(vkEndCommandBuffer(cmd));
}

// ─── drawFrame ────────────────────────────────────────────────────────────────

/**
 * @brief Submit and present one frame.
 * @param window The GLFW window to query for cursor position and key states.
 *               Passed in from main() each frame; not stored as a member.
 */
void Renderer::drawFrame(GLFWwindow* window)
{
    // ── Update the camera from GLFW input ─────────────────────────────────────
    //
    // Called FIRST so the view matrix reflects the latest input before the UBO is
    // written and before the command buffer is recorded with the new descriptor set.
    // The window pointer arrives here from main() — it is only ever used at GLFW
    // C API call sites and is never stored beyond this stack frame.
    updateCamera(window);

    // ── Write the camera matrices into the UBO for this frame slot ────────────
    //
    // uploadUbo writes the view+proj matrices to the HOST_COHERENT buffer for
    // _currentFrame. The GPU will read this buffer during the draw call below.
    // This must happen before vkQueueSubmit but after updateCamera() so the latest
    // camera state is captured.
    uploadUbo(_currentFrame);

    FrameSync& sync = (*_syncPool)[_currentFrame];

    // ── Wait for previous use of this frame slot ──────────────────────────────
    //
    // vkWaitForFences: block the CPU until the GPU signals the inFlight fence.
    // Ensures the command buffer for this slot finished executing on the GPU
    // before we re-record it.
    VK_CHECK(vkWaitForFences(
        _device->device(), 1, &sync.inFlight, VK_TRUE, UINT64_MAX));

    // ── Acquire the next swapchain image ──────────────────────────────────────
    //
    // vkAcquireNextImageKHR: ask the swapchain for the next available image index.
    // The GPU signals imageAvailable when the image is truly ready (not being
    // scanned out by the display hardware).
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
    VK_CHECK(vkResetFences(_device->device(), 1, &sync.inFlight));

    // ── Record the command buffer for this image and frame slot ──────────────
    //
    // The new recordCommandBuffer signature includes frameIndex so it can bind
    // the correct descriptor set (the one pointing at _uniformBuffers[_currentFrame]).
    recordCommandBuffer(imageIndex, _currentFrame);

    // ── Submit the command buffer ─────────────────────────────────────────────
    //
    // vkQueueSubmit: hand the recorded command buffer to the GPU for execution.
    //   waitSemaphores:   imageAvailable — GPU waits until the swapchain image is ready.
    //   signalSemaphores: renderFinished — GPU signals this when rendering is done.
    //   signalFences:     inFlight — GPU signals this when the submission completes.
    //   waitDstStageMask: COLOR_ATTACHMENT_OUTPUT — wait applies at colour write stage.
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSemaphore renderFinished     = _renderFinishedSemaphores[imageIndex];

    VkSubmitInfo submitInfo{};
    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount   = 1;
    submitInfo.pWaitSemaphores      = &sync.imageAvailable;
    submitInfo.pWaitDstStageMask    = &waitStage;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &_commandBuffers[imageIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = &renderFinished;

    VK_CHECK(vkQueueSubmit(_device->graphicsQueue(), 1, &submitInfo, sync.inFlight));

    // ── Present the rendered image ────────────────────────────────────────────
    //
    // vkQueuePresentKHR: ask the swapchain to display the rendered image.
    // waitSemaphores: renderFinished — the swapchain waits until rendering completes.
    // Physically: the display hardware starts scanning out the new image on the next
    // vertical blank (vsync, because VK_PRESENT_MODE_FIFO_KHR is selected).
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &renderFinished;
    presentInfo.swapchainCount     = 1;
    const VkSwapchainKHR sc        = _swapchain->swapchain();
    presentInfo.pSwapchains        = &sc;
    presentInfo.pImageIndices      = &imageIndex;

    VK_CHECK(vkQueuePresentKHR(_device->graphicsQueue(), &presentInfo));

    // Advance to the next frame-in-flight slot.
    _currentFrame = (_currentFrame + 1) % kMaxFramesInFlight;
}
