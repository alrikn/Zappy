/**
 * @file renderer/device/AllocatorContext.hpp
 * @brief RAII wrapper that owns a single VmaAllocator handle.
 * @details VulkanMemoryAllocator (VMA) replaces the three-step manual allocation dance
 *          Vulkan normally requires: query VkMemoryRequirements, find a compatible
 *          memory type via vkGetPhysicalDeviceMemoryProperties, then call
 *          vkAllocateMemory and vkBindImageMemory/vkBindBufferMemory. VMA reduces that
 *          to a single call (vmaCreateImage or vmaCreateBuffer) and tracks the resulting
 *          VmaAllocation so the inverse single call (vmaDestroyImage / vmaDestroyBuffer)
 *          frees everything together.
 *
 *          This thin wrapper exists to keep DeviceContext's single responsibility
 *          (instance, physical device, logical device, queues) intact. Every future
 *          class that needs to allocate GPU memory (vertex buffers, textures, staging
 *          buffers) calls allocator() on this object to get the shared VmaAllocator.
 *
 *          Architecture: created in Renderer immediately after DeviceContext exists.
 *          Declared in Renderer AFTER _device (so it is destroyed BEFORE _device by
 *          C++ reverse-destruction), because the VmaAllocator holds references to
 *          VkInstance, VkPhysicalDevice, and VkDevice — all owned by DeviceContext.
 */

#pragma once

#include <vk_mem_alloc.h>

/**
 * @brief Owns a VmaAllocator: creates it on construction, destroys it on destruction.
 * @details Lifetime: created in Renderer after DeviceContext. Destroyed before
 *          DeviceContext (C++ reverse member-destruction order enforces this when the
 *          member declarations in Renderer.hpp are ordered correctly).
 *
 *          The VmaAllocator is a global-ish resource from VMA's perspective — it does
 *          not own any individual allocation, but it owns the bookkeeping state (pools,
 *          statistics, free lists) shared by all allocations made through it.
 *
 *          Non-copyable, non-movable: the VmaAllocator is an opaque handle that must
 *          not be duplicated.
 *
 *          Thread-safety: VMA itself is internally thread-safe for concurrent
 *          allocation/deallocation calls. This wrapper class is not thread-safe — the
 *          Renderer is a single-threaded object.
 */
class AllocatorContext {
public:
    /**
     * @brief Construct a VmaAllocator bound to the given Vulkan triple.
     * @details Calls vmaCreateAllocator with the minimum required fields: instance,
     *          physical device, logical device, and the Vulkan API version. VMA uses
     *          these to query memory properties and set up its internal sub-allocator.
     * @param instance       The VkInstance (VMA needs it to load function pointers).
     * @param physicalDevice The VkPhysicalDevice (VMA queries its memory type table).
     * @param device         The VkDevice (VMA calls vkAllocateMemory through it).
     * @throws RendererInitException if vmaCreateAllocator returns a non-success result.
     */
    AllocatorContext(VkInstance       instance,
                     VkPhysicalDevice physicalDevice,
                     VkDevice         device);

    /**
     * @brief Destroy the VmaAllocator, releasing its internal bookkeeping state.
     * @details Calls vmaDestroyAllocator. All VmaAllocations made through this allocator
     *          must already have been freed (i.e. DepthResources must be destroyed first)
     *          before this destructor runs. The Renderer member declaration order
     *          guarantees that DepthResources (_depth) is destroyed before
     *          AllocatorContext (_allocator).
     */
    ~AllocatorContext();

    AllocatorContext(const AllocatorContext&)            = delete; ///< Non-copyable.
    AllocatorContext& operator=(const AllocatorContext&) = delete; ///< Non-copyable.
    AllocatorContext(AllocatorContext&&)                 = delete; ///< Non-movable.
    AllocatorContext& operator=(AllocatorContext&&)      = delete; ///< Non-movable.

    /**
     * @brief Return the VmaAllocator handle for use by allocation owners.
     * @details The returned handle is valid for the lifetime of this AllocatorContext.
     *          Callers (e.g. DepthResources) must not store or use this handle after
     *          this AllocatorContext has been destroyed.
     * @return The VmaAllocator handle. Never VK_NULL_HANDLE after successful construction.
     */
    [[nodiscard]] VmaAllocator allocator() const noexcept;

private:
    VmaAllocator _allocator{VK_NULL_HANDLE}; ///< The VMA allocator handle — owned here.
};
