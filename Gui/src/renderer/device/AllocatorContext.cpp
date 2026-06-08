/**
 * @file renderer/device/AllocatorContext.cpp
 * @brief Implementation of AllocatorContext: VmaAllocator creation and destruction.
 * @details VMA requires exactly one VmaAllocator per VkDevice. This file implements
 *          the minimal VmaAllocatorCreateInfo configuration needed to bootstrap VMA
 *          for image and buffer allocation.
 *
 *          VMA_IMPLEMENTATION must be defined in exactly one translation unit before
 *          including vk_mem_alloc.h. That unit becomes the definition site for all of
 *          VMA's implementation code (the header is dual-mode: declaration-only without
 *          the macro, definition-heavy with it). This file is that one translation unit.
 *
 *          Architecture: no other .cpp file in the project defines VMA_IMPLEMENTATION.
 *          All other files include vk_mem_alloc.h without the macro, getting only
 *          declarations (function prototypes, struct definitions).
 */

// VMA_IMPLEMENTATION must be defined before including vk_mem_alloc.h in exactly
// one translation unit. It enables the function definitions inside the header.
// Every other translation unit includes vk_mem_alloc.h without this macro.
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include "renderer/device/AllocatorContext.hpp"
#include "renderer/device/VkCheck.hpp"
#include "exceptions.hpp"

#include <spdlog/spdlog.h>

/**
 * @brief Construct a VmaAllocator bound to the given Vulkan triple.
 * @param instance       The VkInstance.
 * @param physicalDevice The VkPhysicalDevice.
 * @param device         The VkDevice.
 */
AllocatorContext::AllocatorContext(VkInstance       instance,
                                   VkPhysicalDevice physicalDevice,
                                   VkDevice         device)
{
    // VmaAllocatorCreateInfo: the configuration for the VMA allocator.
    // VMA uses the three handles to:
    //   - instance:       load Vulkan extension functions via vkGetInstanceProcAddr
    //   - physicalDevice: query VkPhysicalDeviceMemoryProperties (the GPU's memory
    //                     type table, listing which types are device-local, host-visible, etc.)
    //   - device:         call vkAllocateMemory / vkFreeMemory on behalf of callers
    //
    // vulkanApiVersion: must match the version used when creating the VkInstance.
    // Mismatching this can cause VMA to attempt calling API functions that do not exist
    // on the instance, leading to a null-function-pointer crash.
    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.instance         = instance;
    allocatorInfo.physicalDevice   = physicalDevice;
    allocatorInfo.device           = device;
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_2;

    // vmaCreateAllocator: initialises VMA's internal state — memory type cache,
    // free-list heaps, statistics counters. Does NOT allocate any GPU memory yet.
    // Physically: purely CPU-side work; queries the GPU's memory properties table
    // and sets up data structures to manage future sub-allocations.
    // Skipping this: every subsequent vmaCreateImage / vmaCreateBuffer would have
    // no allocator to route through and would fail immediately.
    const VkResult result = vmaCreateAllocator(&allocatorInfo, &_allocator);
    if (result != VK_SUCCESS) {
        throw RendererInitException("vmaCreateAllocator failed");
    }

    spdlog::info("AllocatorContext: VmaAllocator created.");
}

/**
 * @brief Destroy the VmaAllocator.
 */
AllocatorContext::~AllocatorContext()
{
    if (_allocator != VK_NULL_HANDLE) {
        // vmaDestroyAllocator: frees all VMA-internal bookkeeping structures.
        // Physically: releases CPU-side pool metadata. Any GPU memory that was
        // allocated but not freed before this call becomes a permanent leak — VMA
        // logs a warning in debug mode if un-freed allocations remain.
        vmaDestroyAllocator(_allocator);
        spdlog::debug("AllocatorContext: VmaAllocator destroyed.");
    }
}

/**
 * @brief Return the VmaAllocator handle.
 * @return The VmaAllocator, valid for the lifetime of this object.
 */
VmaAllocator AllocatorContext::allocator() const noexcept
{
    return _allocator;
}
