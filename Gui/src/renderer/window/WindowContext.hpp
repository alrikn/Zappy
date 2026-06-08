/**
 * @file renderer/window/WindowContext.hpp
 * @brief RAII owner of the Vulkan surface (VkSurfaceKHR) for a GLFW window.
 * @details The VkSurfaceKHR is platform-specific (X11/Wayland/Win32) but Vulkan hides
 *          all platform details behind a single KHR extension handle. GLFW creates the
 *          correct platform surface via glfwCreateWindowSurface.
 *
 *          Destruction order constraint: the surface must be destroyed before the
 *          VkInstance. In Renderer, WindowContext is therefore declared after (and thus
 *          destroyed before) DeviceContext.
 *
 *          Architecture: created by Renderer immediately after DeviceContext::createInstance()
 *          returns; the surface it creates is then passed to DeviceContext's constructor
 *          for device selection.
 *
 *          Raw pointer policy: this class does NOT store the GLFWwindow* pointer in any
 *          member. The pointer is accepted by the constructor and used only at the C API
 *          call site (glfwCreateWindowSurface) during construction. framebufferSize()
 *          requires the caller to pass the pointer each time it is needed, ensuring the
 *          raw pointer never persists beyond the scope of a single C API call.
 */

#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

/**
 * @brief Owns a VkSurfaceKHR for the lifetime of the associated GLFW window.
 * @details Lifetime: constructed once in Renderer constructor; destroyed in Renderer
 *          destructor before DeviceContext (surface must die before instance).
 *          Non-copyable, non-movable.
 *
 *          The GLFWwindow* is NOT stored as a member — it is only ever used at the
 *          call site of a GLFW C API function. Callers that need framebuffer dimensions
 *          must pass the window pointer to framebufferSize() each time.
 *
 *          Thread-safety: not thread-safe. All GLFW and Vulkan calls must come from
 *          the main thread.
 */
class WindowContext {
public:
    /**
     * @brief Create the Vulkan surface for the given GLFW window.
     * @details glfwCreateWindowSurface abstracts the platform-specific surface creation
     *          (VkXlibSurfaceCreateInfoKHR on X11, VkWaylandSurfaceCreateInfoKHR on Wayland,
     *          VkWin32SurfaceCreateInfoKHR on Windows). The caller provides the already-
     *          created GLFW window and the Vulkan instance. The window pointer is used only
     *          at this call site and is never stored as a member.
     * @param window   Raw pointer to the GLFW window; used only during this constructor
     *                 call to invoke glfwCreateWindowSurface, never stored.
     * @param instance The VkInstance that will own this surface.
     * @throws RendererVkException if glfwCreateWindowSurface fails.
     */
    WindowContext(GLFWwindow* window, VkInstance instance);

    /**
     * @brief Destroy the VkSurfaceKHR. The GLFWwindow is not destroyed here.
     * @details vkDestroySurfaceKHR must be called before vkDestroyInstance.
     *          The GLFW window outlives WindowContext (it is owned by main()).
     */
    ~WindowContext();

    WindowContext(const WindowContext&)            = delete; ///< Non-copyable.
    WindowContext& operator=(const WindowContext&) = delete; ///< Non-copyable.
    WindowContext(WindowContext&&)                 = delete; ///< Non-movable.
    WindowContext& operator=(WindowContext&&)      = delete; ///< Non-movable.

    /**
     * @brief Return the VkSurfaceKHR for swapchain creation and queue-family queries.
     * @return Surface handle valid for the lifetime of this object.
     */
    [[nodiscard]] VkSurfaceKHR surface() const noexcept;

    /**
     * @brief Query the current framebuffer pixel dimensions from the given GLFW window.
     * @details On HiDPI displays the pixel dimensions may be larger than the window
     *          coordinate dimensions. Always use this method (not window size) when
     *          creating swapchains or viewports.
     *          The window pointer is passed in by the caller each time so that this
     *          object never stores a raw pointer as a member.
     * @param window    Raw pointer to the GLFW window; used only at the glfwGetFramebufferSize
     *                  call site, never stored.
     * @param outWidth  Receives the current framebuffer width in pixels.
     * @param outHeight Receives the current framebuffer height in pixels.
     */
    void framebufferSize(GLFWwindow* window,
                         uint32_t&   outWidth,
                         uint32_t&   outHeight) const noexcept;

private:
    VkInstance   _instance{VK_NULL_HANDLE}; ///< Borrowed reference — not owned here.
    VkSurfaceKHR _surface{VK_NULL_HANDLE};  ///< Created here, destroyed here.
};
