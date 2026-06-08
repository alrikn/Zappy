/**
 * @file renderer/sync/FrameSync.cpp
 * @brief Implementation of FrameSyncPool: semaphore and fence creation/destruction.
 * @details Vulkan synchronisation objects are created with simple, uniform create-infos.
 *          The only non-trivial detail is that fences are created in the *signalled* state
 *          (VK_FENCE_CREATE_SIGNALED_BIT). This prevents the first frame from deadlocking:
 *          drawFrame() calls vkWaitForFences before submitting work; if the fence started
 *          unsignalled, the wait on frame 0 would block forever because no GPU work has
 *          been submitted yet to signal it.
 *
 *          Architecture: only Renderer uses this file. No other translation unit needs
 *          to know about per-frame synchronisation details.
 */

#include "renderer/sync/FrameSync.hpp"
#include "renderer/device/VkCheck.hpp"
#include <spdlog/spdlog.h>

/**
 * @brief Create count FrameSync objects (2 semaphores + 1 fence per frame).
 * @param device The logical device.
 * @param count  Number of frames to keep in flight simultaneously.
 */
FrameSyncPool::FrameSyncPool(VkDevice device, std::size_t count)
    : _device(device)
{
    _frames.resize(count);

    // VkSemaphoreCreateInfo: semaphores have no configurable parameters beyond sType.
    // Their initial state is always "unsignalled". The GPU sets them signalled when
    // the corresponding pipeline stage completes.
    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    // VkFenceCreateInfo: created with SIGNALED_BIT so the first vkWaitForFences call
    // in drawFrame() returns immediately. Without this bit the fence starts unsignalled
    // and the first frame blocks forever waiting for GPU work that was never submitted.
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (std::size_t i = 0; i < count; ++i) {
        // VkSemaphore: GPU-GPU synchronisation primitive.
        // Physically: the GPU writes a completion signal to a location in GPU memory;
        // other GPU queues or stages poll that location before proceeding.
        // imageAvailable: signalled by vkAcquireNextImageKHR → waited on by the graphics queue.
        VK_CHECK(vkCreateSemaphore(_device, &semInfo, nullptr, &_frames[i].imageAvailable));

        // renderFinished: signalled by the graphics queue at submission completion →
        // waited on by vkQueuePresentKHR before flipping the image to the display.
        VK_CHECK(vkCreateSemaphore(_device, &semInfo, nullptr, &_frames[i].renderFinished));

        // VkFence: CPU-GPU synchronisation primitive.
        // Physically: the GPU writes a completion signal to a CPU-visible memory location;
        // the CPU calls vkWaitForFences which polls or blocks until the signal arrives.
        // inFlight: the CPU waits on this at frame start to ensure the command buffer
        // for this slot is no longer being read by the GPU before re-recording.
        // Skipping this wait: the CPU overwrites a command buffer the GPU is still reading
        // → corrupted draw commands → undefined rendering or GPU crash.
        VK_CHECK(vkCreateFence(_device, &fenceInfo, nullptr, &_frames[i].inFlight));
    }

    spdlog::debug("FrameSyncPool: created {} frame-sync slots.", count);
}

/**
 * @brief Destroy all semaphores and fences in all frame slots.
 */
FrameSyncPool::~FrameSyncPool()
{
    for (FrameSync& fs : _frames) {
        if (fs.imageAvailable != VK_NULL_HANDLE) {
            vkDestroySemaphore(_device, fs.imageAvailable, nullptr);
        }
        if (fs.renderFinished != VK_NULL_HANDLE) {
            vkDestroySemaphore(_device, fs.renderFinished, nullptr);
        }
        if (fs.inFlight != VK_NULL_HANDLE) {
            vkDestroyFence(_device, fs.inFlight, nullptr);
        }
    }
    spdlog::debug("FrameSyncPool: all sync objects destroyed.");
}

/**
 * @brief Access the FrameSync for a given in-flight slot.
 * @param index Frame-in-flight index in [0, count()).
 * @return Reference to the FrameSync at that slot.
 */
FrameSync& FrameSyncPool::operator[](std::size_t index)
{
    return _frames[index];
}

/**
 * @brief Return the number of frames in flight.
 * @return The count passed to the constructor.
 */
std::size_t FrameSyncPool::count() const noexcept
{
    return _frames.size();
}
