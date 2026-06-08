/**
 * @file renderer/sync/FrameSync.hpp
 * @brief Per-frame synchronisation primitives: semaphores and fences.
 * @details Vulkan requires explicit synchronisation between CPU and GPU work, and
 *          between GPU stages. This file defines the two types needed for the frame loop:
 *
 *            - FrameSync struct: holds the three objects for one in-flight frame slot.
 *            - FrameSyncPool class: owns a fixed-size array of FrameSync objects.
 *
 *          Architecture: created by Renderer after DeviceContext. Destroyed before
 *          DeviceContext (semaphores and fences are children of the logical device).
 */

#pragma once

#include <cstddef>
#include <vector>
#include <vulkan/vulkan.h>

/**
 * @brief Synchronisation objects for one in-flight frame.
 * @details Lifetime: owned in a vector inside FrameSyncPool. Created once at
 *          renderer startup; destroyed at renderer teardown by FrameSyncPool::~FrameSyncPool().
 *
 *          imageAvailable — signalled by vkAcquireNextImageKHR when the swapchain image
 *                           has been acquired and is ready for the GPU to render into.
 *                           The graphics queue waits on this before executing draw commands.
 *
 *          renderFinished — signalled by vkQueueSubmit (via its signal semaphore) when all
 *                           draw commands in the submission have completed. The present queue
 *                           waits on this before flipping the image to the display.
 *
 *          inFlight       — CPU-GPU fence; the CPU waits on this at the start of each frame
 *                           to ensure the command buffer for this slot is no longer in use by
 *                           the GPU before we re-record it. Created in the signalled state so
 *                           the first frame does not deadlock waiting for a frame that never ran.
 */
struct FrameSync {
    VkSemaphore imageAvailable{VK_NULL_HANDLE}; ///< GPU signal: swapchain image is ready to render.
    VkSemaphore renderFinished{VK_NULL_HANDLE}; ///< GPU signal: rendering is complete, safe to present.
    VkFence     inFlight{VK_NULL_HANDLE};       ///< CPU fence: previous frame's GPU work is done.
};

/**
 * @brief Allocates and owns a fixed array of FrameSync objects.
 * @details Lifetime: created once in Renderer after DeviceContext. Destroyed before
 *          DeviceContext — semaphores and fences belong to the logical device and must
 *          be destroyed before vkDestroyDevice is called.
 *          Non-copyable, non-movable.
 *
 *          Thread-safety: not thread-safe. All accesses must come from the main thread.
 */
class FrameSyncPool {
public:
    /**
     * @brief Create count FrameSync objects (2 semaphores + 1 fence per frame).
     * @param device The logical device.
     * @param count  Number of frames to keep in flight simultaneously (typically 2).
     * @throws RendererVkException if any semaphore or fence creation fails.
     */
    FrameSyncPool(VkDevice device, std::size_t count);

    /**
     * @brief Destroy all semaphores and fences in all frame slots.
     */
    ~FrameSyncPool();

    FrameSyncPool(const FrameSyncPool&)            = delete; ///< Non-copyable.
    FrameSyncPool& operator=(const FrameSyncPool&) = delete; ///< Non-copyable.
    FrameSyncPool(FrameSyncPool&&)                 = delete; ///< Non-movable.
    FrameSyncPool& operator=(FrameSyncPool&&)      = delete; ///< Non-movable.

    /**
     * @brief Access the FrameSync for a given in-flight slot.
     * @param index Frame-in-flight index in [0, count()).
     * @return Reference to the FrameSync at that slot.
     */
    [[nodiscard]] FrameSync& operator[](std::size_t index);

    /**
     * @brief Return the number of frames in flight.
     * @return The count passed to the constructor.
     */
    [[nodiscard]] std::size_t count() const noexcept;

private:
    VkDevice               _device{VK_NULL_HANDLE}; ///< Borrowed — not owned.
    std::vector<FrameSync> _frames;                  ///< One FrameSync per frame in flight.
};
