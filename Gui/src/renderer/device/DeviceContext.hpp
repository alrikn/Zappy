/**
 * @file renderer/device/DeviceContext.hpp
 * @brief Owns the Vulkan instance, physical device, logical device, and queues.
 * @details Uses vk-bootstrap for instance and device selection boilerplate.
 *          Exposes the raw handles needed by downstream Vulkan objects (swapchain,
 *          pipelines, command pools, etc.).
 *
 *          Architecture: created in Renderer after WindowContext, because the surface
 *          is required to verify that the chosen physical device supports presentation.
 *          Declared as the last member in Renderer so it is the last object to be
 *          destroyed (after all child objects that reference its handles).
 */

#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>

/**
 * @brief Aggregates the Vulkan instance, physical device, logical device, and queues.
 * @details Lifetime: created once in the Renderer constructor; destroyed last in the
 *          Renderer destructor. All child objects (swapchain, pipelines, sync primitives,
 *          command pools) must be destroyed before this object, because they hold handles
 *          that reference the VkDevice and VkInstance owned here.
 *
 *          Non-copyable and non-movable: Vulkan handles are opaque 64-bit integers backed
 *          by GPU-side allocations. Duplicating them without Vulkan's involvement is
 *          undefined behaviour.
 *
 *          Thread-safety: not thread-safe. All calls must originate from the main thread
 *          that created the Vulkan objects.
 */
class DeviceContext {
public:
    /**
     * @brief Create a VkInstance using vk-bootstrap, with validation layers in Debug builds.
     * @details This static factory is called by Renderer before constructing WindowContext,
     *          because glfwCreateWindowSurface (called inside WindowContext) needs an
     *          existing VkInstance. The instance is then threaded back into this class's
     *          constructor along with the surface.
     *
     *          In Debug builds (VULKAN_VALIDATION=1), the VK_LAYER_KHRONOS_validation layer
     *          is enabled. This layer intercepts every Vulkan call and validates parameters,
     *          usage flags, and synchronisation — at zero cost in Release builds.
     * @return The created VkInstance. Caller (Renderer) must pass it to the constructor and
     *         ensure it lives until this DeviceContext is destroyed.
     * @throws RendererInitException if vk-bootstrap fails to build the instance.
     */
    static VkInstance createInstance();

    /**
     * @brief Select a physical device and create a logical device with a graphics+present queue.
     * @details Uses vk-bootstrap's PhysicalDeviceSelector to enumerate GPUs and pick the
     *          best one that supports both graphics commands and presentation on the given
     *          surface. A logical device (VkDevice) is then created with one queue from the
     *          selected queue family.
     * @param instance The VkInstance created by createInstance().
     * @param surface  The VkSurfaceKHR used to verify present support during device selection.
     * @throws RendererInitException if vk-bootstrap cannot select a suitable device or
     *         build the logical device.
     */
    DeviceContext(VkInstance instance, VkSurfaceKHR surface);

    /**
     * @brief Destroy the logical device, then the debug messenger, then the instance.
     * @details Vulkan requires: vkDestroyDevice → vkDestroyDebugUtilsMessengerEXT →
     *          vkDestroyInstance. Reversing this order is undefined behaviour.
     *          vkb::destroy_device and vkb::destroy_instance handle this ordering.
     */
    ~DeviceContext();

    DeviceContext(const DeviceContext&)            = delete; ///< Non-copyable.
    DeviceContext& operator=(const DeviceContext&) = delete; ///< Non-copyable.
    DeviceContext(DeviceContext&&)                 = delete; ///< Non-movable.
    DeviceContext& operator=(DeviceContext&&)      = delete; ///< Non-movable.

    /**
     * @brief Return the VkInstance handle.
     * @return VkInstance valid for the lifetime of this object.
     */
    [[nodiscard]] VkInstance        instance()            const noexcept;

    /**
     * @brief Return the selected VkPhysicalDevice.
     * @return VkPhysicalDevice valid for the lifetime of this object.
     */
    [[nodiscard]] VkPhysicalDevice  physicalDevice()      const noexcept;

    /**
     * @brief Return the VkDevice (logical device).
     * @return VkDevice valid for the lifetime of this object.
     */
    [[nodiscard]] VkDevice          device()              const noexcept;

    /**
     * @brief Return the combined graphics+present queue handle.
     * @return VkQueue valid for the lifetime of this object.
     */
    [[nodiscard]] VkQueue           graphicsQueue()       const noexcept;

    /**
     * @brief Return the queue family index for the graphics+present queue.
     * @return Queue family index used when creating command pools and submitting work.
     */
    [[nodiscard]] uint32_t          graphicsQueueFamily() const noexcept;

private:
    VkInstance               _instance{VK_NULL_HANDLE};       ///< Root Vulkan object.
    VkPhysicalDevice         _physicalDevice{VK_NULL_HANDLE}; ///< Selected GPU handle.
    VkDevice                 _device{VK_NULL_HANDLE};          ///< Logical device handle.
    VkQueue                  _graphicsQueue{VK_NULL_HANDLE};   ///< Combined graphics+present queue.
    uint32_t                 _graphicsQueueFamily{0};           ///< Queue family index.

    /// Debug messenger handle; VK_NULL_HANDLE in Release builds.
    VkDebugUtilsMessengerEXT _debugMessenger{VK_NULL_HANDLE};
};
