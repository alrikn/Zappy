/**
 * @file renderer/scene/DepthResources.hpp
 * @brief Owns the depth image, its GPU memory allocation, and its image view.
 * @details A depth buffer is a 2D image sized identically to the swapchain. For every
 *          pixel the GPU renders, it stores a distance value (depth) in this image. At
 *          the end of a frame, each pixel in the colour image contains the colour of the
 *          geometry that was closest to the camera — the depth buffer is what enforces
 *          that "closest wins" rule.
 *
 *          This class bundles three resources that must always exist, match each other,
 *          and die together:
 *            - VkImage:        the raw texel storage on the GPU.
 *            - VmaAllocation:  the GPU memory range backing the image (managed by VMA).
 *            - VkImageView:    the "lens" the render pass uses to read/write the image.
 *
 *          Architecture: created in Renderer after AllocatorContext (needs the allocator)
 *          and before Pipeline (Pipeline needs format() to declare the depth attachment
 *          in the render pass). Must be destroyed before AllocatorContext (still holds
 *          a VmaAllocation). Declared in Renderer between _allocator and _pipeline.
 */

#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

/**
 * @brief Creates and owns a depth image + GPU memory + image view for depth testing.
 * @details Lifetime: created once in the Renderer constructor after AllocatorContext
 *          and before Pipeline. Destroyed before AllocatorContext (the VmaAllocation it
 *          holds must be freed through the VmaAllocator that AllocatorContext owns).
 *          Sized to match the swapchain extent; must be re-created if the swapchain is
 *          re-created (swapchain resize is not yet implemented — this note is for future
 *          reference).
 *
 *          Non-copyable, non-movable.
 *
 *          Thread-safety: not thread-safe; all calls from the main thread only.
 */
class DepthResources {
public:
    /**
     * @brief Select a supported depth format, allocate the depth image via VMA,
     *        and create a VkImageView with VK_IMAGE_ASPECT_DEPTH_BIT.
     * @details Format selection probes a candidate list in order of preference:
     *          VK_FORMAT_D32_SFLOAT → VK_FORMAT_D32_SFLOAT_S8_UINT →
     *          VK_FORMAT_D24_UNORM_S8_UINT. The first format that the physical device
     *          reports as supporting VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
     *          (in its optimalTilingFeatures) is chosen.
     * @param allocator      The VmaAllocator to allocate the image through.
     * @param device         The logical device (for vkCreateImageView / vkDestroyImageView).
     * @param physicalDevice The physical device (for format-support queries).
     * @param width          The swapchain image width in pixels.
     * @param height         The swapchain image height in pixels.
     * @throws RendererInitException if no depth format is supported by the physical device.
     * @throws RendererVkException   on any VkResult failure (image or view creation).
     */
    DepthResources(VmaAllocator     allocator,
                   VkDevice         device,
                   VkPhysicalDevice physicalDevice,
                   uint32_t         width,
                   uint32_t         height);

    /**
     * @brief Destroy the image view, then free the image and its GPU memory via VMA.
     * @details vkDestroyImageView is called first (the view must be destroyed before the
     *          image it references). vmaDestroyImage then frees both the VkImage and the
     *          VmaAllocation in one call — the inverse of vmaCreateImage.
     */
    ~DepthResources();

    DepthResources(const DepthResources&)            = delete; ///< Non-copyable.
    DepthResources& operator=(const DepthResources&) = delete; ///< Non-copyable.
    DepthResources(DepthResources&&)                 = delete; ///< Non-movable.
    DepthResources& operator=(DepthResources&&)      = delete; ///< Non-movable.

    /**
     * @brief Return the VkImageView for the depth image.
     * @details Passed to SwapchainContext so each framebuffer can bind the depth view
     *          as its second attachment (attachment index 1, matching the render pass
     *          depth attachment declaration in Pipeline).
     * @return VkImageView valid for the lifetime of this object.
     */
    [[nodiscard]] VkImageView view()   const noexcept;

    /**
     * @brief Return the chosen depth format.
     * @details Passed to Pipeline so the render pass depth attachment description uses
     *          exactly the same format as the actual image. A mismatch would trigger a
     *          Vulkan validation error at vkCmdBeginRenderPass time.
     * @return VkFormat, one of the candidates from the probe list.
     */
    [[nodiscard]] VkFormat    format() const noexcept;

private:
    VmaAllocator  _allocator{VK_NULL_HANDLE};    ///< Borrowed — not owned.
    VkDevice      _device{VK_NULL_HANDLE};       ///< Borrowed — not owned.
    VkImage       _image{VK_NULL_HANDLE};        ///< Owned (via VMA — destroyed with vmaDestroyImage).
    VmaAllocation _allocation{VK_NULL_HANDLE};   ///< GPU memory — freed with vmaDestroyImage.
    VkImageView   _view{VK_NULL_HANDLE};         ///< Owned — destroyed with vkDestroyImageView.
    VkFormat      _format{VK_FORMAT_UNDEFINED};  ///< The selected depth format.
};
