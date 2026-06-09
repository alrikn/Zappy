/**
 * @file renderer/scene/DescriptorAllocator.cpp
 * @brief Implementation of DescriptorAllocator: pool creation, set allocation, buffer binding.
 * @details The descriptor system is Vulkan's mechanism for telling shaders where their data
 *          lives. The chain is:
 *
 *            VkDescriptorSetLayout → describes what binding slots exist (type + stage)
 *            VkDescriptorPool      → pre-allocated reservoir of descriptor slots
 *            VkDescriptorSet       → a concrete bundle of buffer/image bindings
 *            vkUpdateDescriptorSets → wires a VkBuffer address into binding slot 0
 *
 *          All three Vulkan calls are made once at startup. During rendering, only
 *          vkCmdBindDescriptorSets is called (once per frame, in the command buffer)
 *          — the descriptor set contents never change after construction.
 *
 *          Architecture: this file includes only Vulkan headers, UniformBuffer.hpp,
 *          and VkCheck.hpp. No Renderer or Pipeline headers are included here.
 */

#include "renderer/scene/DescriptorAllocator.hpp"
#include "renderer/device/VkCheck.hpp"

#include <spdlog/spdlog.h>

// ─── constructor ──────────────────────────────────────────────────────────────

/**
 * @brief Create the pool, allocate one set per frame slot, and bind each set to its buffer.
 * @param device         The logical device.
 * @param setLayout      The VkDescriptorSetLayout that describes binding 0 (uniform buffer).
 * @param uniformBuffers Per-frame UniformBuffer objects — one set is created per entry.
 */
DescriptorAllocator::DescriptorAllocator(VkDevice                           device,
                                         VkDescriptorSetLayout              setLayout,
                                         const std::vector<UniformBuffer*>& uniformBuffers)
    : _device(device)
{
    const uint32_t count = static_cast<uint32_t>(uniformBuffers.size());
    _sets.resize(count);

    // ── Step 1: Descriptor pool creation ─────────────────────────────────────
    //
    // VkDescriptorPool: a pre-allocated reservoir from which descriptor sets are
    // handed out. Instead of calling vkAllocateMemory for each descriptor (which
    // would be expensive), Vulkan reserves a pool of descriptor slots up front.
    //
    // VkDescriptorPoolSize: declares how many descriptors of a given type to reserve.
    // type = UNIFORM_BUFFER: we need one uniform buffer binding per descriptor set,
    // and we create one set per frame-in-flight slot.
    //
    // maxSets: the maximum total number of descriptor sets allocatable from this pool.
    // We allocate exactly count sets (one per frame slot) — setting maxSets = count.
    //
    // Physically: the driver allocates a fixed block of GPU-accessible memory that can
    // hold `count` uniform buffer descriptor entries. Each entry is a small record
    // (a buffer address + size) that the shader hardware reads at draw time to locate
    // the UBO data. Skipping the pool would leave nowhere to allocate sets from —
    // vkAllocateDescriptorSets would fail with VK_ERROR_OUT_OF_POOL_MEMORY.
    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = count;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    poolInfo.maxSets       = count;

    VK_CHECK(vkCreateDescriptorPool(_device, &poolInfo, nullptr, &_pool));
    spdlog::debug("DescriptorAllocator: VkDescriptorPool created ({} sets).", count);

    // ── Step 2: Descriptor set allocation ────────────────────────────────────
    //
    // VkDescriptorSet: the concrete binding bundle. Before a draw call, the command
    // buffer records vkCmdBindDescriptorSets to make a set active for the pipeline.
    // The shader then reads from binding 0 to find the UBO buffer address.
    //
    // We allocate count sets at once (one vkAllocateDescriptorSets call) using the
    // same layout repeated count times. All sets are compatible with the same layout,
    // so each set has exactly one binding: a uniform buffer at slot 0.
    //
    // Physically: each set occupies a slot in the pool's reserved memory. The GPU's
    // shader unit looks up the set by index when vkCmdBindDescriptorSets is called,
    // then reads the buffer address stored at binding 0 to fetch UBO data from RAM.
    std::vector<VkDescriptorSetLayout> layouts(count, setLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = _pool;
    allocInfo.descriptorSetCount = count;
    allocInfo.pSetLayouts        = layouts.data();

    VK_CHECK(vkAllocateDescriptorSets(_device, &allocInfo, _sets.data()));
    spdlog::debug("DescriptorAllocator: {} VkDescriptorSets allocated.", count);

    // ── Step 3: Update each descriptor set to point at its UniformBuffer ─────
    //
    // vkUpdateDescriptorSets: writes the actual buffer address into each descriptor
    // set slot. Until this call, the set exists but its binding 0 points nowhere.
    // After this call, when the GPU executes a draw with this set bound, it reads
    // the UboMvp struct from the corresponding UniformBuffer's VkBuffer.
    //
    // VkDescriptorBufferInfo: a small struct that pins down exactly which region of
    // which VkBuffer a uniform binding should point at.
    //   buffer: the VkBuffer handle.
    //   offset: byte offset into the buffer (0 = start).
    //   range:  number of bytes the shader may read (sizeof(UboMvp) = 128 bytes).
    //
    // VkWriteDescriptorSet: one write operation — updates one binding in one set.
    // dstBinding = 0: matches "layout(binding=0)" in the vertex shader.
    // dstArrayElement = 0: not an array descriptor — update the single element.
    // descriptorType = UNIFORM_BUFFER: must match the pool reservation and the layout.
    //
    // Physically: the driver writes the buffer handle and range into the GPU-readable
    // descriptor pool memory. When the vertex shader runs and reads from binding 0,
    // the shader unit follows this stored pointer to fetch the UboMvp struct bytes.
    // Updating a descriptor set after a draw has started (without synchronisation)
    // is a validation error — but here we update at startup, before any draw occurs.
    for (uint32_t i = 0; i < count; ++i) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers[i]->buffer();
        bufferInfo.offset = 0;
        bufferInfo.range  = sizeof(UboMvp);

        VkWriteDescriptorSet write{};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = _sets[i];
        write.dstBinding      = 0;
        write.dstArrayElement = 0;
        write.descriptorCount = 1;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.pBufferInfo     = &bufferInfo;

        // vkUpdateDescriptorSets: not a VkResult-returning call — it cannot fail.
        // It writes directly into the descriptor pool memory; no VK_CHECK needed.
        vkUpdateDescriptorSets(_device, 1, &write, 0, nullptr);
    }

    spdlog::debug("DescriptorAllocator: all sets updated with UniformBuffer bindings.");
}

// ─── destructor ───────────────────────────────────────────────────────────────

/**
 * @brief Destroy the descriptor pool, implicitly freeing all allocated sets.
 */
DescriptorAllocator::~DescriptorAllocator()
{
    // vkDestroyDescriptorPool: frees the pool and implicitly invalidates every
    // VkDescriptorSet allocated from it. No need to call vkFreeDescriptorSets first.
    if (_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(_device, _pool, nullptr);
        spdlog::debug("DescriptorAllocator: VkDescriptorPool destroyed.");
    }
}

// ─── accessor ─────────────────────────────────────────────────────────────────

/**
 * @brief Return the descriptor set for a given frame-in-flight slot.
 * @param frameIndex Index in [0, N) where N is the number of UniformBuffers provided.
 * @return VkDescriptorSet bound to the UniformBuffer for that slot.
 */
VkDescriptorSet DescriptorAllocator::set(std::size_t frameIndex) const
{
    return _sets[frameIndex];
}
