/**
 * @file renderer/scene/DescriptorAllocator.hpp
 * @brief Declares DescriptorAllocator: owns the descriptor pool and per-frame descriptor sets.
 * @details A descriptor set is how the GPU learns where to find the UBO data at draw time.
 *          This class creates one pool large enough for kMaxFramesInFlight uniform-buffer
 *          descriptors, allocates one VkDescriptorSet per frame slot from that pool, and
 *          immediately binds each set to its corresponding UniformBuffer. After construction
 *          the sets are ready to be bound with vkCmdBindDescriptorSets — only the buffer
 *          contents change each frame, not the bindings.
 *
 *          Architecture: owned by Renderer as a unique_ptr<DescriptorAllocator>, declared
 *          after _uniformBuffers (construction requires their VkBuffer handles) and before
 *          _camera (no ordering constraint on camera). Destroying the pool automatically
 *          frees all sets allocated from it, so the destructor only calls vkDestroyDescriptorPool.
 */

#pragma once

#include <cstddef>
#include <vector>
#include <vulkan/vulkan.h>

#include "renderer/scene/UniformBuffer.hpp"

/**
 * @brief Owns a VkDescriptorPool and allocates one VkDescriptorSet per frame-in-flight slot.
 * @details At construction time, a pool is created with capacity for N uniform-buffer
 *          descriptors (where N = number of UniformBuffer objects passed in), then one
 *          VkDescriptorSet is allocated per slot and immediately updated to point at the
 *          corresponding UniformBuffer. This is the standard Vulkan pattern for UBOs that
 *          change every frame: allocate once, update once, then just swap which buffer the
 *          already-bound set points at by choosing the correct set per frame.
 *
 *          The raw UniformBuffer pointers passed to the constructor are used only during
 *          construction (to fill VkDescriptorBufferInfo). They are NOT stored as members;
 *          there is no lifetime dependency after the constructor returns.
 *
 *          Lifetime: created in the Renderer constructor after all UniformBuffers are ready.
 *          Destroyed by unique_ptr in the Renderer destructor. Destruction happens before
 *          the UniformBuffers are destroyed (declaration order in Renderer ensures this).
 *
 *          Non-copyable, non-movable.
 *          Thread-safety: not thread-safe. All calls from the main thread only.
 */
class DescriptorAllocator {
public:
    /**
     * @brief Create the pool, allocate one set per frame slot, and bind each set to its buffer.
     * @details Internally calls vkCreateDescriptorPool, vkAllocateDescriptorSets (once), and
     *          vkUpdateDescriptorSets (once per slot) to wire each VkDescriptorSet to the
     *          VkBuffer from the corresponding UniformBuffer. After this call, each set is
     *          ready to be bound with vkCmdBindDescriptorSets during command buffer recording.
     * @param device         The logical device.
     * @param setLayout      The VkDescriptorSetLayout created by Pipeline. Used both to
     *                       allocate compatible sets and to verify binding compatibility.
     *                       Must outlive this object.
     * @param uniformBuffers Per-frame UniformBuffer objects. One VkDescriptorSet is created
     *                       per entry. Raw pointers are used only during construction — not stored.
     * @throws RendererVkException on any Vulkan call failure.
     */
    DescriptorAllocator(VkDevice                           device,
                        VkDescriptorSetLayout              setLayout,
                        const std::vector<UniformBuffer*>& uniformBuffers);

    /**
     * @brief Destroy the descriptor pool, implicitly freeing all sets allocated from it.
     * @details Calling vkDestroyDescriptorPool automatically invalidates every
     *          VkDescriptorSet that was allocated from it. There is no need to call
     *          vkFreeDescriptorSets first.
     */
    ~DescriptorAllocator();

    DescriptorAllocator(const DescriptorAllocator&)            = delete; ///< Non-copyable.
    DescriptorAllocator& operator=(const DescriptorAllocator&) = delete; ///< Non-copyable.
    DescriptorAllocator(DescriptorAllocator&&)                 = delete; ///< Non-movable.
    DescriptorAllocator& operator=(DescriptorAllocator&&)      = delete; ///< Non-movable.

    /**
     * @brief Return the descriptor set for a given frame-in-flight slot.
     * @param frameIndex The frame-in-flight index in [0, N) where N is the number of
     *                   UniformBuffers passed to the constructor.
     * @return VkDescriptorSet bound to the UniformBuffer for that slot.
     */
    [[nodiscard]] VkDescriptorSet set(std::size_t frameIndex) const;

private:
    VkDevice                     _device{VK_NULL_HANDLE}; ///< Borrowed — not owned.
    VkDescriptorPool             _pool{VK_NULL_HANDLE};   ///< Owned here — destroyed in ~DescriptorAllocator().
    std::vector<VkDescriptorSet> _sets;                   ///< One per frame-in-flight; owned by the pool.
};
