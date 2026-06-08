/**
 * @file renderer/window/WindowContext.cpp
 * @brief Implementation of WindowContext: VkSurfaceKHR creation and lifecycle.
 * @details glfwCreateWindowSurface is the only cross-platform way to create a Vulkan
 *          surface for a GLFW window. It selects the correct platform extension at
 *          runtime (VK_KHR_xlib_surface, VK_KHR_xcb_surface, VK_KHR_wayland_surface,
 *          or VK_KHR_win32_surface) and handles the platform-specific create-info struct.
 *
 *          Architecture: this file has no knowledge of the swapchain or the pipeline.
 *          It owns exactly one Vulkan object (the surface). The raw GLFWwindow* pointer
 *          is never stored — it is used only at the C API call site in the constructor
 *          and in framebufferSize(), where the caller provides it each time.
 */

#include "renderer/window/WindowContext.hpp"
#include "renderer/device/VkCheck.hpp"
#include <spdlog/spdlog.h>

/**
 * @brief Create the Vulkan surface for the given GLFW window.
 * @param window   Raw pointer to the GLFW window; used only to call
 *                 glfwCreateWindowSurface at this call site, never stored.
 * @param instance The VkInstance that will own this surface.
 */
WindowContext::WindowContext(GLFWwindow* window, VkInstance instance)
    : _instance(instance)
{
    // glfwCreateWindowSurface: the cross-platform surface factory.
    //
    // Physically: GLFW queries the OS for the native window handle (e.g. the X11
    // Window integer on Linux) and fills a platform-specific VkSurfaceCreateInfoKHR.
    // The Vulkan loader then creates the surface object in the driver, which records
    // the native handle so it can later configure the swapchain to present to it.
    //
    // Skipping this step: the swapchain has no window to present to; vkQueuePresentKHR
    // would have no valid surface and would return VK_ERROR_SURFACE_LOST_KHR.
    //
    // The raw pointer is used only here — it is not stored in any member.
    VK_CHECK(glfwCreateWindowSurface(_instance, window, nullptr, &_surface));

    spdlog::debug("WindowContext: VkSurfaceKHR created.");
}

/**
 * @brief Destroy the VkSurfaceKHR. The GLFWwindow is not destroyed here.
 */
WindowContext::~WindowContext()
{
    // vkDestroySurfaceKHR: releases the driver's record of the surface.
    // Must be called before vkDestroyInstance — the instance owns the surface KHR extension.
    if (_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(_instance, _surface, nullptr);
        spdlog::debug("WindowContext: VkSurfaceKHR destroyed.");
    }
    // The GLFWwindow is not destroyed here — it is owned by main()'s unique_ptr.
}

/**
 * @brief Return the VkSurfaceKHR handle.
 * @return Surface handle valid for the lifetime of this object.
 */
VkSurfaceKHR WindowContext::surface() const noexcept
{
    return _surface;
}

/**
 * @brief Query the current framebuffer pixel dimensions from the given GLFW window.
 * @param window    Raw pointer to the GLFW window; used only at the
 *                  glfwGetFramebufferSize call site, never stored.
 * @param outWidth  Receives the current framebuffer width in pixels.
 * @param outHeight Receives the current framebuffer height in pixels.
 */
void WindowContext::framebufferSize(GLFWwindow* window,
                                    uint32_t&   outWidth,
                                    uint32_t&   outHeight) const noexcept
{
    int w{0};
    int h{0};
    // glfwGetFramebufferSize returns pixel dimensions, which may differ from
    // window coordinates on HiDPI (Retina) displays.
    glfwGetFramebufferSize(window, &w, &h);
    outWidth  = static_cast<uint32_t>(w);
    outHeight = static_cast<uint32_t>(h);
}
