/**
 * @file renderer/scene/UniformBuffer.hpp
 * @brief Declares UniformBuffer: a persistently-mapped, host-coherent VMA buffer for UBO data.
 * @details One UniformBuffer instance is created per frame-in-flight slot. Each frame writes its
 *          UboMvp data into its own buffer before submitting draw commands, so the GPU reading
 *          frame N's buffer does not race with the CPU writing frame N+1's buffer.
 *
 *          Architecture: owned by Renderer as a vector<unique_ptr<UniformBuffer>>. The vector
 *          is declared after _syncPool so it is destroyed before _syncPool but after _allocator
 *          (the VmaAllocator must outlive every VmaAllocation made through it).
 *          DescriptorAllocator borrows the VkBuffer handles at construction time only.
 */

#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include "renderer/scene/UboMvp.hpp"

/**
 * @brief Owns a host-visible, persistently-mapped VMA buffer for one UBO slot.
 * @details Memory type: VMA_MEMORY_USAGE_CPU_TO_GPU with
 *          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT.
 *          HOST_VISIBLE means the CPU can write to it directly through a mapped pointer.
 *          HOST_COHERENT means writes become visible to the GPU without an explicit
 *          vkFlushMappedMemoryRanges call — the CPU store and the GPU load are automatically
 *          synchronised at the cache level.
 *
 *          The buffer uses VMA_ALLOCATION_CREATE_MAPPED_BIT so VMA keeps it persistently
 *          mapped. The mapped pointer is retrieved via vmaGetAllocationInfo() on each
 *          write() call rather than stored as a member — raw C API pointers must not be
 *          stored in members (CLAUDE.md Rule 1).
 *
 *          Lifetime: created in the Renderer constructor (one instance per frame-in-flight
 *          slot), destroyed in the Renderer destructor via unique_ptr. The VmaAllocator
 *          must outlive this object.
 *
 *          Non-copyable, non-movable.
 *
 *          Thread-safety: not thread-safe. All calls from the main thread only.
 */
class UniformBuffer {
public:
    /**
     * @brief Allocate the VMA buffer and request a persistent CPU-side mapping.
     * @details Creates a HOST_VISIBLE | HOST_COHERENT buffer of size sizeof(UboMvp)
     *          through VMA with VMA_ALLOCATION_CREATE_MAPPED_BIT. VMA keeps the mapping
     *          alive for the lifetime of the allocation; write() retrieves it on demand
     *          via vmaGetAllocationInfo(). No pointer is stored as a member.
     * @param allocator The VmaAllocator to allocate through. Must outlive this object.
     * @throws RendererVkException if vmaCreateBuffer fails.
     * @throws RendererInitException if vmaCreateBuffer returns a null pMappedData.
     */
    explicit UniformBuffer(VmaAllocator allocator);

    /**
     * @brief Destroy the buffer and its VmaAllocation.
     * @details Calls vmaDestroyBuffer to free both the VkBuffer handle and the underlying
     *          GPU memory. Because VMA_ALLOCATION_CREATE_MAPPED_BIT was used at creation,
     *          VMA unmaps the memory implicitly — no explicit vmaUnmapMemory call is needed.
     */
    ~UniformBuffer();

    UniformBuffer(const UniformBuffer&)            = delete; ///< Non-copyable.
    UniformBuffer& operator=(const UniformBuffer&) = delete; ///< Non-copyable.
    UniformBuffer(UniformBuffer&&)                 = delete; ///< Non-movable.
    UniformBuffer& operator=(UniformBuffer&&)      = delete; ///< Non-movable.

    /**
     * @brief Write a UboMvp value into the persistently-mapped buffer.
     * @details Calls vmaGetAllocationInfo() to obtain the current mapped pointer, then
     *          performs a raw memcpy of sizeof(UboMvp) bytes. Safe to call every frame
     *          because HOST_COHERENT guarantees the GPU sees the write as soon as the CPU
     *          store completes — no explicit flush is required. The pointer is not stored
     *          between calls; it is fetched fresh each time to avoid holding a raw C API
     *          pointer as a class member (CLAUDE.md Rule 1).
     * @param data The UboMvp struct containing view and projection matrices to write.
     */
    void write(const UboMvp& data);

    /**
     * @brief Return the underlying VkBuffer handle.
     * @details Needed when writing the VkDescriptorBufferInfo for the descriptor set
     *          update. Valid for the lifetime of this UniformBuffer object.
     * @return VkBuffer handle; valid until this object is destroyed.
     */
    [[nodiscard]] VkBuffer buffer() const noexcept;

private:
    VmaAllocator  _allocator{VK_NULL_HANDLE};   ///< Borrowed — not owned.
    VkBuffer      _buffer{VK_NULL_HANDLE};      ///< Owned via VMA (freed by vmaDestroyBuffer).
    VmaAllocation _allocation{VK_NULL_HANDLE};  ///< GPU memory backing the buffer — freed with vmaDestroyBuffer.
    /// No _mapped member: VMA_ALLOCATION_CREATE_MAPPED_BIT keeps the memory permanently
    /// mapped; write() retrieves the pointer on demand via vmaGetAllocationInfo() and uses
    /// it only at the call site. Storing it as a member would violate Rule 1 (no raw C API
    /// pointers stored in members).
};
