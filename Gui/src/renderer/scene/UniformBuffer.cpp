/**
 * @file renderer/scene/UniformBuffer.cpp
 * @brief Implementation of UniformBuffer: VMA buffer allocation, persistent map, write, teardown.
 * @details The central idea: allocating a HOST_VISIBLE | HOST_COHERENT buffer once at startup
 *          with VMA_ALLOCATION_CREATE_MAPPED_BIT keeps it mapped for its entire lifetime. The GPU
 *          can read the buffer contents after every CPU memcpy without any synchronisation call
 *          because HOST_COHERENT means cache-line flushes happen automatically.
 *
 *          The mapped pointer is retrieved via vmaGetAllocationInfo() inside write() on every
 *          call rather than cached in a member. This satisfies Rule 1: raw C API pointers must
 *          not be stored in class members — only used at the call site. The overhead of one
 *          vmaGetAllocationInfo() call per frame is negligible (it is a struct copy, not a
 *          system call), and it ensures correctness if VMA ever needs to remap the allocation.
 *
 *          Architecture: this translation unit only touches VMA and the UboMvp struct.
 *          It does not include Renderer.hpp or any other subsystem headers, keeping
 *          compile-time dependencies minimal.
 */

#include "renderer/scene/UniformBuffer.hpp"
#include "renderer/device/VkCheck.hpp"
#include "exceptions.hpp"

#include <cstring>
#include <spdlog/spdlog.h>

// ─── constructor ──────────────────────────────────────────────────────────────

/**
 * @brief Allocate the VMA buffer and establish the persistent mapping.
 * @param allocator The VmaAllocator to use for allocation. Must outlive this object.
 */
UniformBuffer::UniformBuffer(VmaAllocator allocator)
    : _allocator(allocator)
{
    // ── Buffer creation ───────────────────────────────────────────────────────
    //
    // VkBuffer: a linear array of bytes on the GPU. Unlike VkImage (which has
    // tiling, format, and aspect), a buffer is just raw addressable storage.
    // We use it here to hold the UboMvp struct (128 bytes: two mat4 values).
    //
    // VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT: declares this buffer as a uniform
    // buffer. The shader declares "layout(binding=0) uniform UboMvp { ... }" and
    // the GPU reads data from this buffer during each vertex shader invocation.
    // Without this bit the Vulkan validation layers reject binding the buffer
    // to a uniform descriptor.
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size        = sizeof(UboMvp);
    bufferInfo.usage       = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    // EXCLUSIVE: only the graphics queue accesses this buffer. CONCURRENT would
    // be needed if multiple queue families accessed it simultaneously, which adds
    // overhead and is not needed here.
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    // VmaAllocationCreateInfo: instruct VMA on what memory properties we need.
    //
    // VMA_MEMORY_USAGE_CPU_TO_GPU: data flows from CPU (we write every frame)
    // to GPU (the vertex shader reads it). VMA selects a memory type that is:
    //   HOST_VISIBLE (CPU can map and write to it directly)
    //   HOST_COHERENT (writes are visible to the GPU without an explicit flush)
    //
    // VMA_ALLOCATION_CREATE_MAPPED_BIT: request a persistent mapping immediately
    // at allocation time. VMA stores the mapped pointer in the VmaAllocationInfo
    // and we can retrieve it after the call. This replaces a separate vmaMapMemory
    // call and avoids the map/unmap overhead on every write.
    //
    // Physically: on most desktop GPUs (NVIDIA, AMD), HOST_VISIBLE | HOST_COHERENT
    // memory lives in a region that is both accessible by the CPU through the PCIe
    // BAR and readable by the GPU through the cache-coherent fabric. Writes from
    // the CPU side become visible to the GPU after the CPU store instruction
    // completes — no fence or barrier is needed between the memcpy and the draw call
    // as long as the descriptor set was updated before recording the draw command.
    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    // vmaCreateBuffer: allocates the VkBuffer and its backing memory in one call.
    // Returns both the VkBuffer handle and the VmaAllocation that tracks the memory.
    // The VmaAllocationInfo contains the persistent map pointer when MAPPED_BIT is set.
    // We pass a local VmaAllocationInfo here only to validate that the mapping was
    // established at construction time — we do NOT store pMappedData as a member.
    // Storing the raw void* would violate Rule 1; instead write() fetches it on demand.
    VmaAllocationInfo allocationInfo{};
    const VkResult result = vmaCreateBuffer(
        _allocator, &bufferInfo, &allocInfo, &_buffer, &_allocation, &allocationInfo);
    if (result != VK_SUCCESS) {
        throw RendererVkException(result, "vmaCreateBuffer (uniform buffer)");
    }

    // Validate the persistent mapping was established. Null here means VMA failed to
    // map despite MAPPED_BIT — should never happen with CPU_TO_GPU memory on any
    // supported GPU, but fail fast rather than silently memcpy into a null pointer later.
    if (allocationInfo.pMappedData == nullptr) {
        throw RendererInitException(
            "UniformBuffer: vmaCreateBuffer returned null pMappedData despite MAPPED_BIT");
    }

    spdlog::debug("UniformBuffer: created (size={} bytes, persistently mapped).",
                  sizeof(UboMvp));
}

// ─── destructor ───────────────────────────────────────────────────────────────

/**
 * @brief Unmap and destroy the buffer and its VmaAllocation.
 */
UniformBuffer::~UniformBuffer()
{
    // vmaDestroyBuffer: the inverse of vmaCreateBuffer. Calls vkDestroyBuffer and
    // then frees the GPU memory backing the allocation in one step.
    // The persistent mapping established at creation is implicitly invalidated when
    // the memory is freed — we do not need to call vmaUnmapMemory separately when
    // VMA_ALLOCATION_CREATE_MAPPED_BIT was used.
    if (_buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(_allocator, _buffer, _allocation);
        spdlog::debug("UniformBuffer: destroyed.");
    }
}

// ─── public interface ─────────────────────────────────────────────────────────

/**
 * @brief Write a UboMvp value into the persistently-mapped buffer memory.
 * @param data The UboMvp struct to copy into GPU-visible memory.
 */
void UniformBuffer::write(const UboMvp& data)
{
    // vmaGetAllocationInfo: retrieve the current state of this allocation from VMA.
    // The pMappedData field contains the persistent CPU-writable pointer established
    // at creation by VMA_ALLOCATION_CREATE_MAPPED_BIT. Fetching it here (rather than
    // storing it as a member) avoids holding a raw void* in the class, which would
    // violate Rule 1. The call is a struct copy — no system call, negligible overhead.
    //
    // Physically: pMappedData points into a PCIe BAR-mapped region that is simultaneously
    // readable by the GPU. Writing to it is a normal CPU store; HOST_COHERENT ensures the
    // GPU sees the updated bytes without a vkFlushMappedMemoryRanges call.
    VmaAllocationInfo info{};
    vmaGetAllocationInfo(_allocator, _allocation, &info);

    // memcpy: copy the UboMvp bytes into the CPU-writable GPU memory region.
    // HOST_COHERENT guarantees the GPU sees the updated bytes as soon as this
    // store completes — no explicit flush is required.
    std::memcpy(info.pMappedData, &data, sizeof(UboMvp));
}

/**
 * @brief Return the VkBuffer handle.
 * @return VkBuffer valid until this object is destroyed.
 */
VkBuffer UniformBuffer::buffer() const noexcept
{
    return _buffer;
}
