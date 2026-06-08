/**
 * @file renderer/window/SwapchainContext.hpp
 * @brief Owns the VkSwapchainKHR, per-image VkImageViews, and VkFramebuffers.
 * @details The swapchain is a ring of GPU images that rotate between "being rendered
 *          into" and "being displayed on screen". This class wraps the full lifecycle:
 *          swapchain creation (via vk-bootstrap), colour image view creation (one per
 *          swapchain image), and framebuffer creation (one per swapchain image, bound to
 *          the provided render pass with both colour and depth attachments).
 *
 *          Each VkFramebuffer references two image views:
 *            - Attachment 0: per-swapchain-image colour view (owned here).
 *            - Attachment 1: shared depth view (owned by DepthResources, borrowed here).
 *          This ordering must match the attachment declarations in Pipeline's render pass.
 *
 *          Destruction order constraint: framebuffers reference the render pass and the
 *          depth view. The render pass must outlive this object (Pipeline is declared
 *          before SwapchainContext in Renderer, so it is destroyed after). The depth view
 *          must outlive this object (DepthResources is declared before SwapchainContext
 *          in Renderer, so it is destroyed after SwapchainContext).
 *
 *          Architecture: created by Renderer after Pipeline and DepthResources, because
 *          both the render pass handle and the depth image view are needed for framebuffer
 *          creation.
 */

#pragma once

#include <cstdint>
#include <vector>
#include <vulkan/vulkan.h>

/**
 * @brief Manages the swapchain, per-image colour views, and per-image framebuffers.
 * @details Lifetime: created in Renderer constructor after Pipeline and DepthResources;
 *          destroyed before both (because framebuffers reference the render pass owned by
 *          Pipeline and the depth view owned by DepthResources).
 *          Non-copyable, non-movable.
 *
 *          Thread-safety: not thread-safe. All calls must come from the main (render) thread.
 */
class SwapchainContext {
public:
    /**
     * @brief Create the swapchain, retrieve images, create colour views and framebuffers.
     * @details Uses vk-bootstrap's SwapchainBuilder to select the best format, present mode,
     *          and extent. Then creates one colour VkImageView per swapchain image and one
     *          VkFramebuffer per colour view, each with two attachments: the colour view and
     *          the shared depth view.
     * @param device         The logical device.
     * @param physicalDevice The physical device (for surface capability queries).
     * @param surface        The window surface (determines format and present mode).
     * @param renderPass     The render pass — framebuffers are created compatible with it.
     * @param depthView      The depth image view (attachment 1) — borrowed, not owned.
     *                       Must remain valid for the lifetime of this object.
     * @param width          Desired swapchain image width in pixels.
     * @param height         Desired swapchain image height in pixels.
     * @throws RendererVkException on any Vulkan call failure.
     * @throws RendererInitException if vk-bootstrap fails to build the swapchain.
     */
    SwapchainContext(VkDevice         device,
                     VkPhysicalDevice physicalDevice,
                     VkSurfaceKHR     surface,
                     VkRenderPass     renderPass,
                     VkImageView      depthView,
                     uint32_t         width,
                     uint32_t         height);

    /**
     * @brief Destroy framebuffers, image views, and the swapchain.
     * @details Images are owned by the swapchain and must NOT be destroyed explicitly.
     */
    ~SwapchainContext();

    SwapchainContext(const SwapchainContext&)            = delete; ///< Non-copyable.
    SwapchainContext& operator=(const SwapchainContext&) = delete; ///< Non-copyable.
    SwapchainContext(SwapchainContext&&)                 = delete; ///< Non-movable.
    SwapchainContext& operator=(SwapchainContext&&)      = delete; ///< Non-movable.

    /**
     * @brief Return the VkSwapchainKHR handle.
     * @return Swapchain handle valid for the lifetime of this object.
     */
    [[nodiscard]] VkSwapchainKHR swapchain()   const noexcept;

    /**
     * @brief Return the number of swapchain images (and therefore framebuffers).
     * @return Image count chosen by the swapchain builder (typically 2 or 3).
     */
    [[nodiscard]] uint32_t       imageCount()  const noexcept;

    /**
     * @brief Return the VkFormat of the swapchain images.
     * @return The selected surface format (e.g. VK_FORMAT_B8G8R8A8_SRGB).
     */
    [[nodiscard]] VkFormat        imageFormat() const noexcept;

    /**
     * @brief Return the swapchain image width in pixels.
     * @return Width in pixels.
     */
    [[nodiscard]] uint32_t        width()       const noexcept;

    /**
     * @brief Return the swapchain image height in pixels.
     * @return Height in pixels.
     */
    [[nodiscard]] uint32_t        height()      const noexcept;

    /**
     * @brief Return the framebuffer at the given swapchain image index.
     * @param index Swapchain image index in [0, imageCount()).
     * @return VkFramebuffer for that image index.
     * @throws RendererInitException if index is out of bounds.
     */
    [[nodiscard]] VkFramebuffer   framebuffer(uint32_t index) const;

private:
    VkDevice _device{VK_NULL_HANDLE};                 ///< Borrowed — not owned.
    VkSwapchainKHR _swapchain{VK_NULL_HANDLE};        ///< The swapchain handle — owned here.
    VkFormat       _imageFormat{VK_FORMAT_UNDEFINED}; ///< Selected surface format.
    uint32_t       _width{0};                          ///< Image width in pixels.
    uint32_t       _height{0};                         ///< Image height in pixels.

    std::vector<VkImage>       _images;       ///< Swapchain images — owned by the swapchain, not us.
    std::vector<VkImageView>   _imageViews;   ///< One view per image — owned here.
    std::vector<VkFramebuffer> _framebuffers; ///< One framebuffer per view — owned here.
};
