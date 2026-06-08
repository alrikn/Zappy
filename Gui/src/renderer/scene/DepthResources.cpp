/**
 * @file renderer/scene/DepthResources.cpp
 * @brief Implementation of DepthResources: depth format selection, image allocation, view creation.
 * @details Three steps are performed in the constructor:
 *
 *          1. FORMAT SELECTION — probe a candidate list via vkGetPhysicalDeviceFormatProperties.
 *             Not every GPU supports every depth format. The preferred candidate is
 *             VK_FORMAT_D32_SFLOAT (32-bit float depth, no stencil), which is supported
 *             on all modern desktop GPUs. VK_FORMAT_D32_SFLOAT_S8_UINT and
 *             VK_FORMAT_D24_UNORM_S8_UINT are fallbacks for hardware that only supports
 *             combined depth+stencil formats. If none match, a RendererInitException is thrown.
 *
 *          2. IMAGE ALLOCATION — call vmaCreateImage with VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT.
 *             VMA internally calls vkCreateImage, queries VkMemoryRequirements, selects a
 *             device-local memory type, calls vkAllocateMemory, and calls vkBindImageMemory —
 *             all in one step. The VkImage and VmaAllocation are returned together; they must
 *             be freed together with vmaDestroyImage.
 *
 *          3. VIEW CREATION — call vkCreateImageView with VK_IMAGE_ASPECT_DEPTH_BIT.
 *             The subresourceRange aspect must use DEPTH_BIT (not COLOR_BIT) so the GPU
 *             knows this view is a depth surface when it is used as a framebuffer attachment.
 *
 *          Architecture: no other file creates a depth image. DepthResources is the
 *          single owner; Pipeline and SwapchainContext borrow format() and view() respectively.
 */

#include "renderer/scene/DepthResources.hpp"
#include "renderer/device/VkCheck.hpp"
#include "exceptions.hpp"

#include <array>
#include <spdlog/spdlog.h>

// ─── constructor ──────────────────────────────────────────────────────────────

/**
 * @brief Select depth format, allocate image via VMA, create image view.
 * @param allocator      The VmaAllocator.
 * @param device         The logical device.
 * @param physicalDevice The physical device (format query target).
 * @param width          Swapchain image width in pixels.
 * @param height         Swapchain image height in pixels.
 */
DepthResources::DepthResources(VmaAllocator     allocator,
                               VkDevice         device,
                               VkPhysicalDevice physicalDevice,
                               uint32_t         width,
                               uint32_t         height)
    : _allocator(allocator)
    , _device(device)
{
    // ── Step 1: Depth format selection ───────────────────────────────────────
    //
    // Not every GPU supports every depth format as an optimal-tiling
    // depth/stencil attachment. The application must query which formats
    // the physical device supports and pick the best available one.
    //
    // VK_FORMAT_D32_SFLOAT: 32-bit float depth, no stencil.
    //   Preferred: widest precision, no stencil channel overhead.
    // VK_FORMAT_D32_SFLOAT_S8_UINT: 32-bit float depth + 8-bit stencil.
    //   Fallback 1: stencil data wastes a byte per pixel but is still acceptable.
    // VK_FORMAT_D24_UNORM_S8_UINT: 24-bit normalized depth + 8-bit stencil.
    //   Fallback 2: older GPUs that do not support pure float depth.
    //
    // We test only optimalTilingFeatures because VkImages created for
    // attachment use are always created with VK_IMAGE_TILING_OPTIMAL.
    // The DEPTH_STENCIL_ATTACHMENT_BIT flag confirms the GPU can use this
    // format as a depth/stencil attachment (write depth values to it).
    const std::array<VkFormat, 3> candidates = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
    };

    for (VkFormat candidate : candidates) {
        // vkGetPhysicalDeviceFormatProperties: ask the GPU "what can you do with
        // this texel format?" Returns three sets of feature flags, one for each
        // tiling mode (linear, optimal) and one for buffer-based usage. We only
        // care about optimalTilingFeatures because our image will be created with
        // TILING_OPTIMAL (the GPU's internally optimal layout for 2D access).
        //
        // Skipping this query and hard-coding a format would fail silently on
        // hardware that does not support it — the image creation call would return
        // VK_ERROR_FORMAT_NOT_SUPPORTED and the validation layers would emit an error.
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(physicalDevice, candidate, &props);

        if (props.optimalTilingFeatures &
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            _format = candidate;
            break;
        }
    }

    if (_format == VK_FORMAT_UNDEFINED) {
        throw RendererInitException(
            "DepthResources: no supported depth format found on this GPU");
    }

    spdlog::info("DepthResources: selected depth format {}",
                 static_cast<int>(_format));

    // ── Step 2: Depth image allocation via VMA ────────────────────────────────
    //
    // VkImage: an N-dimensional GPU texture — here a 2D array of depth texels,
    // one per screen pixel. Unlike a VkBuffer (a flat byte array), a VkImage has
    // a format and a memory tiling layout the GPU optimises for 2D access patterns.
    //
    // VMA's vmaCreateImage is a single call that replaces four raw Vulkan calls:
    //   1. vkCreateImage                     — allocate the image descriptor
    //   2. vkGetImageMemoryRequirements       — query alignment + size + allowed types
    //   3. vkAllocateMemory                  — reserve device-local memory
    //   4. vkBindImageMemory                 — wire the memory to the image
    //
    // Skipping vmaCreateImage (doing it manually) would require all four steps,
    // plus memory-type index lookup via vkGetPhysicalDeviceMemoryProperties —
    // exactly the error-prone bookkeeping VMA exists to eliminate.
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.format        = _format;
    imageInfo.extent.width  = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth  = 1;
    imageInfo.mipLevels     = 1;     // no mipmaps for the depth buffer
    imageInfo.arrayLayers   = 1;     // not a cubemap or array texture
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;  // no MSAA
    // VK_IMAGE_TILING_OPTIMAL: the GPU arranges texels in its internal swizzled
    // layout — fastest for GPU access, not human-readable. Required for
    // DEPTH_STENCIL_ATTACHMENT usage. (LINEAR tiling is for CPU-accessible images.)
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    // VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT: this image will be used as the
    // depth attachment of a render pass. Without this flag the driver cannot set up
    // the hardware depth test unit to write into it.
    imageInfo.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;  // used by one queue family only
    // VK_IMAGE_LAYOUT_UNDEFINED: we do not care about the initial contents —
    // the render pass clears the depth buffer at the start of every frame.
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    // VmaAllocationCreateInfo: tells VMA what kind of memory we want.
    // VMA_MEMORY_USAGE_AUTO: VMA picks the best memory type automatically.
    //   For a depth attachment (device-side only, never read back by the CPU),
    //   VMA will select DEVICE_LOCAL memory — the fastest GPU heap.
    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

    // vmaCreateImage: combines vkCreateImage + memory allocation + vkBindImageMemory.
    // On success: _image is a valid VkImage handle; _allocation tracks the backing
    // memory so vmaDestroyImage can free both together.
    // On failure: throws RendererVkException via the manual result check below.
    const VkResult result = vmaCreateImage(
        _allocator, &imageInfo, &allocInfo, &_image, &_allocation, nullptr);
    if (result != VK_SUCCESS) {
        throw RendererVkException(result, "vmaCreateImage (depth image)");
    }

    spdlog::debug("DepthResources: depth image allocated ({}x{}).", width, height);

    // ── Step 3: Image view creation ───────────────────────────────────────────
    //
    // VkImageView: a typed interpretation of a VkImage's memory. The render pass
    // framebuffer attachment references a view, not the image directly. The view
    // declares which aspect of the image to expose (colour, depth, or stencil).
    //
    // VK_IMAGE_ASPECT_DEPTH_BIT: tell the GPU this view exposes the depth channel.
    // Using COLOR_BIT here would fail with a validation error because the image's
    // format is a depth format (not a colour format), and the framebuffer would
    // receive incorrect data from the depth hardware.
    //
    // Note: if the chosen format contains a stencil component
    // (D32_SFLOAT_S8_UINT or D24_UNORM_S8_UINT), we still only expose the depth
    // aspect here. We do not use stencil testing in this feature; adding
    // VK_IMAGE_ASPECT_STENCIL_BIT to the aspect mask would expose the stencil
    // channel unnecessarily and is not required by the render pass.
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image    = _image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format   = _format;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;

    VK_CHECK(vkCreateImageView(_device, &viewInfo, nullptr, &_view));
    spdlog::debug("DepthResources: VkImageView created.");
}

// ─── destructor ───────────────────────────────────────────────────────────────

/**
 * @brief Destroy the image view, then free the image and its memory via VMA.
 */
DepthResources::~DepthResources()
{
    // Destroy the view before the image it points into.
    if (_view != VK_NULL_HANDLE) {
        vkDestroyImageView(_device, _view, nullptr);
        spdlog::debug("DepthResources: VkImageView destroyed.");
    }

    // vmaDestroyImage: the inverse of vmaCreateImage. Calls vkDestroyImage and
    // then vkFreeMemory on the backing allocation in one call. Must happen before
    // the VmaAllocator itself is destroyed (AllocatorContext destructor).
    if (_image != VK_NULL_HANDLE) {
        vmaDestroyImage(_allocator, _image, _allocation);
        spdlog::debug("DepthResources: depth image and allocation freed.");
    }
}

// ─── accessors ────────────────────────────────────────────────────────────────

/**
 * @brief Return the VkImageView for the depth image.
 * @return View handle, valid for the lifetime of this object.
 */
VkImageView DepthResources::view() const noexcept
{
    return _view;
}

/**
 * @brief Return the chosen depth format.
 * @return VkFormat, one of the three candidates.
 */
VkFormat DepthResources::format() const noexcept
{
    return _format;
}
