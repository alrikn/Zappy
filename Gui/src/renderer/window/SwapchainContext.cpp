/**
 * @file renderer/window/SwapchainContext.cpp
 * @brief Implementation of SwapchainContext: swapchain, image views, framebuffers.
 * @details Uses vk-bootstrap's SwapchainBuilder to handle the boilerplate of querying
 *          surface capabilities, selecting format and present mode, and calling
 *          vkCreateSwapchainKHR.
 *
 *          After the swapchain is created:
 *            1. Retrieve the VkImage handles (owned by the swapchain — do NOT destroy them).
 *            2. Create one VkImageView per image (the view tells Vulkan how to interpret
 *               the image's memory: format, mip levels, array layers, component swizzle).
 *            3. Create one VkFramebuffer per image view (binds the view to the render pass's
 *               colour attachment slot).
 *
 *          Architecture: the only consumer of vk-bootstrap's SwapchainBuilder in the project.
 *          No other file needs to know about vk-bootstrap's swapchain API.
 */

#include "renderer/window/SwapchainContext.hpp"
#include "renderer/device/VkCheck.hpp"
#include "exceptions.hpp"

#include <VkBootstrap.h>
#include <spdlog/spdlog.h>

/**
 * @brief Create the swapchain, retrieve images, create image views and framebuffers.
 * @param device         The logical device.
 * @param physicalDevice The physical device.
 * @param surface        The window surface.
 * @param renderPass     The render pass for framebuffer compatibility.
 * @param width          Desired width in pixels.
 * @param height         Desired height in pixels.
 */
SwapchainContext::SwapchainContext(VkDevice         device,
                                   VkPhysicalDevice physicalDevice,
                                   VkSurfaceKHR     surface,
                                   VkRenderPass     renderPass,
                                   uint32_t         width,
                                   uint32_t         height)
    : _device(device)
    , _width(width)
    , _height(height)
{
    // ── Swapchain creation ────────────────────────────────────────────────────
    //
    // VkSwapchainKHR: the ring of presentable images.
    // Physically: the driver allocates N GPU images and connects them to the OS
    // compositor. Your code renders into image[i]; the compositor scans out
    // image[j]. Double-buffering (N=2) or triple-buffering (N=3) keeps the
    // pipeline full without tearing.
    //
    // vkb::SwapchainBuilder: handles the three-step query process required by
    // Vulkan — query surface capabilities → query surface formats → query present
    // modes — then picks the best combination and calls vkCreateSwapchainKHR.
    vkb::SwapchainBuilder swapBuilder{physicalDevice, device, surface};
    swapBuilder
        .set_desired_extent(width, height)
        // VK_PRESENT_MODE_FIFO_KHR: vsync — the image is presented on the next
        // vertical blank. Guaranteed to be available on all Vulkan implementations.
        // Prevents tearing without wasting GPU cycles on frames the display cannot show.
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        // VK_FORMAT_B8G8R8A8_SRGB: preferred format — 8 bits per channel, SRGB
        // colour space. The GPU hardware performs gamma correction automatically.
        .set_desired_format(VkSurfaceFormatKHR{
            VK_FORMAT_B8G8R8A8_SRGB,
            VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
        .set_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

    auto result = swapBuilder.build();
    if (!result) {
        throw RendererInitException(
            std::string("vkb::SwapchainBuilder failed: ") + result.error().message());
    }

    vkb::Swapchain vkbSwap = result.value();
    _swapchain   = vkbSwap.swapchain;
    _imageFormat = vkbSwap.image_format;
    _width       = vkbSwap.extent.width;
    _height      = vkbSwap.extent.height;

    spdlog::info("SwapchainContext: swapchain created ({}x{}, format={}, images={})",
                 _width, _height,
                 static_cast<int>(_imageFormat),
                 vkbSwap.image_count);

    // ── Retrieve swapchain images ─────────────────────────────────────────────
    //
    // VkImage: a GPU texture object — a 2D array of texels.
    // The swapchain owns these images; we must NOT call vkDestroyImage on them.
    // We only get the handles so we can create views into them.
    auto imagesResult = vkbSwap.get_images();
    if (!imagesResult) {
        throw RendererInitException(
            std::string("vkb::Swapchain::get_images failed: ") +
            imagesResult.error().message());
    }
    _images = imagesResult.value();

    // ── Create image views ────────────────────────────────────────────────────
    //
    // VkImageView: a view into a VkImage that describes how to interpret its memory.
    // Physically: the shader reads through the view — the view tells the GPU the
    // format, how to swizzle channels, and which mip/array slice to access.
    // Skipping views: the render pass attachment references a view, not the image
    // directly; without views the framebuffer cannot be created.
    _imageViews.reserve(_images.size());
    for (VkImage img : _images) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image    = img;
        // VK_IMAGE_VIEW_TYPE_2D: interpret the image as a standard 2D texture.
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format   = _imageFormat;

        // components: identity swizzle — each channel maps to itself (R→R, G→G, etc.).
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        // subresourceRange: which part of the image this view covers.
        // VK_IMAGE_ASPECT_COLOR_BIT: we are viewing the colour data (not depth/stencil).
        // baseMipLevel=0, levelCount=1: only the base mip level (no mipmaps).
        // baseArrayLayer=0, layerCount=1: only the first array layer (not a cubemap).
        viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel   = 0;
        viewInfo.subresourceRange.levelCount     = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount     = 1;

        VkImageView view{VK_NULL_HANDLE};
        VK_CHECK(vkCreateImageView(_device, &viewInfo, nullptr, &view));
        _imageViews.push_back(view);
    }

    // ── Create framebuffers ───────────────────────────────────────────────────
    //
    // VkFramebuffer: binds the abstract attachment slots of a VkRenderPass to concrete
    // VkImageView objects. One framebuffer per swapchain image is the standard pattern.
    // Physically: when vkCmdBeginRenderPass is called with this framebuffer, the GPU
    // routes its output pixels into the referenced image views.
    // Skipping framebuffers: vkCmdBeginRenderPass has no framebuffer → VK_UNDEFINED_BEHAVIOUR.
    _framebuffers.reserve(_imageViews.size());
    for (VkImageView view : _imageViews) {
        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass      = renderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments    = &view;
        fbInfo.width           = _width;
        fbInfo.height          = _height;
        fbInfo.layers          = 1;

        VkFramebuffer fb{VK_NULL_HANDLE};
        VK_CHECK(vkCreateFramebuffer(_device, &fbInfo, nullptr, &fb));
        _framebuffers.push_back(fb);
    }

    spdlog::debug("SwapchainContext: {} image views and {} framebuffers created.",
                  _imageViews.size(), _framebuffers.size());
}

/**
 * @brief Destroy framebuffers, image views, and the swapchain.
 */
SwapchainContext::~SwapchainContext()
{
    // Destroy framebuffers first: they reference the image views.
    for (VkFramebuffer fb : _framebuffers) {
        if (fb != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(_device, fb, nullptr);
        }
    }

    // Destroy image views: they reference the images.
    for (VkImageView view : _imageViews) {
        if (view != VK_NULL_HANDLE) {
            vkDestroyImageView(_device, view, nullptr);
        }
    }

    // _images are owned by the swapchain — do NOT destroy them.

    // vkDestroySwapchainKHR: releases the swapchain and all its images.
    // Must happen before the device is destroyed.
    if (_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(_device, _swapchain, nullptr);
        spdlog::debug("SwapchainContext: swapchain destroyed.");
    }
}

/**
 * @brief Return the VkSwapchainKHR handle.
 * @return Swapchain handle valid for the lifetime of this object.
 */
VkSwapchainKHR SwapchainContext::swapchain() const noexcept
{
    return _swapchain;
}

/**
 * @brief Return the number of swapchain images.
 * @return Count of images (and views and framebuffers).
 */
uint32_t SwapchainContext::imageCount() const noexcept
{
    return static_cast<uint32_t>(_images.size());
}

/**
 * @brief Return the VkFormat of the swapchain images.
 * @return The selected surface format.
 */
VkFormat SwapchainContext::imageFormat() const noexcept
{
    return _imageFormat;
}

/**
 * @brief Return the swapchain image width in pixels.
 * @return Width in pixels.
 */
uint32_t SwapchainContext::width() const noexcept
{
    return _width;
}

/**
 * @brief Return the swapchain image height in pixels.
 * @return Height in pixels.
 */
uint32_t SwapchainContext::height() const noexcept
{
    return _height;
}

/**
 * @brief Return the framebuffer at the given swapchain image index.
 * @param index Swapchain image index in [0, imageCount()).
 * @return VkFramebuffer for that index.
 */
VkFramebuffer SwapchainContext::framebuffer(uint32_t index) const
{
    // Explicit bounds check with a custom exception rather than relying on
    // std::vector::at() which throws std::out_of_range (a stdlib exception not
    // derived from ZappyException — violates Rule 12).
    if (index >= static_cast<uint32_t>(_framebuffers.size())) {
        throw RendererInitException("SwapchainContext::framebuffer() index out of range");
    }
    return _framebuffers[index];
}
