/**
 * @file renderer/device/DeviceContext.cpp
 * @brief Implementation of DeviceContext: instance, physical device, logical device.
 * @details Uses vk-bootstrap's fluent builder API to replace the ~300 lines of raw
 *          Vulkan boilerplate that instance/device selection normally requires.
 *
 *          Vulkan initialisation order enforced here:
 *            1. VkInstance        — root object; everything else depends on it.
 *            2. Debug messenger   — installed on the instance for validation output.
 *            3. VkPhysicalDevice  — enumerated from the instance; not created/destroyed.
 *            4. VkDevice          — created from the physical device; owns queues.
 *
 *          Architecture: DeviceContext.cpp is the only file in the project that pulls in
 *          <VkBootstrap.h>. Keeping the dependency local prevents vk-bootstrap types from
 *          leaking into the public API and lets us swap it out later if needed.
 *
 *          Construction split: DeviceContext::createInstance() is a static factory that
 *          returns only VkInstance (needed by Renderer to construct WindowContext before
 *          the DeviceContext object exists). The DeviceContext constructor then receives
 *          the same instance back along with the surface and completes initialisation.
 *          The debug messenger is created inside the constructor, not in createInstance(),
 *          to avoid any module-level state. This works because the messenger can be
 *          attached to an existing VkInstance after the fact.
 */

#include "renderer/device/DeviceContext.hpp"
#include "renderer/device/VkCheck.hpp"
#include "exceptions.hpp"

#include <VkBootstrap.h>
#include <spdlog/spdlog.h>

// ─── static factory ───────────────────────────────────────────────────────────

/**
 * @brief Create a VkInstance using vk-bootstrap (without a debug messenger).
 * @details Returns the raw VkInstance so Renderer can pass it to WindowContext before
 *          the DeviceContext object is constructed. The debug messenger is set up in
 *          the DeviceContext constructor, not here, so that no module-level state is needed.
 *
 *          In Debug builds (VULKAN_VALIDATION=1), validation layers are requested so
 *          that subsequent Vulkan calls from WindowContext are covered by the validator.
 *          The messenger itself is added in the constructor.
 * @return The created VkInstance.
 * @throws RendererInitException if the instance cannot be built.
 */
VkInstance DeviceContext::createInstance()
{
    // vkb::InstanceBuilder: wraps the VkInstanceCreateInfo boilerplate.
    // It queries available layers and extensions, then calls vkCreateInstance.
    vkb::InstanceBuilder builder;

    builder
        .set_app_name("zappy_gui")
        .set_app_version(VK_MAKE_VERSION(0, 1, 0))
        .require_api_version(VK_API_VERSION_1_2);

#if VULKAN_VALIDATION
    // VK_LAYER_KHRONOS_validation: the official validation layer provided by the
    // Vulkan SDK. It intercepts every Vulkan call and checks parameters, memory
    // usage, synchronisation, and more. Debug-only: zero cost in Release builds.
    //
    // Only request_validation_layers() is called here — do NOT configure a debug
    // callback on the builder. Calling set_debug_callback() causes vk-bootstrap to
    // create a VkDebugUtilsMessengerEXT inside build(), but since only the raw
    // VkInstance is extracted and the vkb::Instance wrapper is discarded, that
    // messenger handle is never destroyed (64-byte LeakSanitizer report).
    // The messenger is created in the constructor instead, stored as _debugMessenger,
    // and destroyed in the correct order in the destructor.
    builder.request_validation_layers();

    spdlog::debug("DeviceContext: validation layers enabled (Debug build).");
#endif

    auto result = builder.build();
    if (!result) {
        throw RendererInitException(
            std::string("vkb::InstanceBuilder failed: ") + result.error().message());
    }

    vkb::Instance vkbInst = result.value();

    spdlog::info("DeviceContext: VkInstance created.");
    return vkbInst.instance;
}

// ─── constructor ──────────────────────────────────────────────────────────────

/**
 * @brief Select a physical device and create a logical device with a graphics+present queue.
 * @param instance The VkInstance created by createInstance().
 * @param surface  The VkSurfaceKHR used to verify present support during device selection.
 */
DeviceContext::DeviceContext(VkInstance instance, VkSurfaceKHR surface)
    : _instance(instance)
{
#if VULKAN_VALIDATION
    // ── Attach a debug messenger to the existing instance ─────────────────────
    //
    // VkDebugUtilsMessengerEXT: captures validation layer messages and routes them
    // to a callback (our spdlog forwarder). Attaching it here (in the constructor
    // rather than in createInstance()) means the messenger handle flows through the
    // normal class member lifecycle — no module-level state is needed.
    //
    // We cannot use vkb::InstanceBuilder here (the instance already exists), so we
    // call the raw extension function directly, loaded via vkGetInstanceProcAddr.
    {
        VkDebugUtilsMessengerCreateInfoEXT messengerInfo{};
        messengerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        messengerInfo.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        messengerInfo.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        messengerInfo.pfnUserCallback =
            [](VkDebugUtilsMessageSeverityFlagBitsEXT severity,
               VkDebugUtilsMessageTypeFlagsEXT,
               const VkDebugUtilsMessengerCallbackDataEXT* data,
               void*) -> VkBool32
            {
                if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
                    spdlog::error("[Vulkan] {}", data->pMessage);
                } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
                    spdlog::warn("[Vulkan] {}", data->pMessage);
                } else {
                    spdlog::debug("[Vulkan] {}", data->pMessage);
                }
                return VK_FALSE;
            };

        // vkCreateDebugUtilsMessengerEXT is a Vulkan extension function — it is not
        // directly exported by libvulkan.so. We must look it up via vkGetInstanceProcAddr.
        auto fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(_instance, "vkCreateDebugUtilsMessengerEXT"));

        if (fn != nullptr) {
            VK_CHECK(fn(_instance, &messengerInfo, nullptr, &_debugMessenger));
            spdlog::debug("DeviceContext: VkDebugUtilsMessengerEXT created.");
        } else {
            spdlog::warn("DeviceContext: vkCreateDebugUtilsMessengerEXT not found — "
                         "validation messages will not appear.");
        }
    }
#endif

    // ── Physical device selection ─────────────────────────────────────────────
    //
    // VkPhysicalDevice: a handle that represents one GPU on the host machine.
    // It is enumerated from the instance — never "created" or "destroyed".
    // Querying it reveals GPU capabilities: memory types, queue families, formats.
    //
    // vkb::PhysicalDeviceSelector: scores every available GPU against our requirements.
    // We require: Vulkan 1.2, a graphics queue family, and present support on the given
    // surface. If multiple GPUs pass, it prefers discrete over integrated GPUs.
    //
    // To use vkb::PhysicalDeviceSelector we need a vkb::Instance wrapper.
    // We reconstruct one from our raw handles — this does not re-create anything.
    vkb::Instance vkbInstWrapper;
    vkbInstWrapper.instance               = _instance;
    vkbInstWrapper.debug_messenger        = _debugMessenger;
    vkbInstWrapper.fp_vkGetInstanceProcAddr =
        reinterpret_cast<PFN_vkGetInstanceProcAddr>(
            vkGetInstanceProcAddr(_instance, "vkGetInstanceProcAddr"));

    vkb::PhysicalDeviceSelector physSelector{vkbInstWrapper};
    physSelector
        .set_surface(surface)
        .set_minimum_version(1, 2)
        .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete);

    auto physResult = physSelector.select();
    if (!physResult) {
        throw RendererInitException(
            std::string("vkb::PhysicalDeviceSelector failed: ") +
            physResult.error().message());
    }

    vkb::PhysicalDevice vkbPhys = physResult.value();
    _physicalDevice = vkbPhys.physical_device;

    spdlog::info("DeviceContext: selected GPU '{}'", vkbPhys.name);

    // ── Logical device creation ───────────────────────────────────────────────
    //
    // VkDevice: your application's interface to one GPU. It owns all queues,
    // command pools, pipelines, and memory allocations you create through it.
    // One logical device corresponds to one physical device.
    //
    // vkb::DeviceBuilder: calls vkCreateDevice with the queue families and device
    // extensions (VK_KHR_swapchain is required for presentation) that we need.
    vkb::DeviceBuilder deviceBuilder{vkbPhys};
    auto devResult = deviceBuilder.build();

    if (!devResult) {
        throw RendererInitException(
            std::string("vkb::DeviceBuilder failed: ") +
            devResult.error().message());
    }

    vkb::Device vkbDev = devResult.value();
    _device = vkbDev.device;

    // ── Queue retrieval ───────────────────────────────────────────────────────
    //
    // VkQueue: a channel through which command buffers are submitted to the GPU.
    // A "graphics" queue can execute draw commands; a "present" queue can flip images
    // to the display. On most consumer GPUs, both capabilities live in one queue family.
    //
    // We request a single queue that supports both. vkb::Device::get_queue() returns
    // the VkQueue handle; get_queue_index() returns the family index we need for
    // command pool creation.
    auto queueResult = vkbDev.get_queue(vkb::QueueType::graphics);
    if (!queueResult) {
        throw RendererInitException(
            std::string("vkb::Device::get_queue(graphics) failed: ") +
            queueResult.error().message());
    }
    _graphicsQueue = queueResult.value();

    auto familyResult = vkbDev.get_queue_index(vkb::QueueType::graphics);
    if (!familyResult) {
        throw RendererInitException(
            std::string("vkb::Device::get_queue_index(graphics) failed: ") +
            familyResult.error().message());
    }
    _graphicsQueueFamily = familyResult.value();

    spdlog::info("DeviceContext: logical device created, graphicsQueueFamily={}",
                 _graphicsQueueFamily);
}

// ─── destructor ───────────────────────────────────────────────────────────────

/**
 * @brief Destroy the logical device, debug messenger, and instance in the correct order.
 */
DeviceContext::~DeviceContext()
{
    // vkDestroyDevice: releases all resources owned by the logical device —
    // queues, pipelines, memory allocations, shader modules. Must be called before
    // vkDestroyInstance; reversing the order is undefined behaviour.
    // All child objects created from this device must already be destroyed by the
    // time this destructor runs (enforced by Renderer member declaration order).
    if (_device != VK_NULL_HANDLE) {
        vkDestroyDevice(_device, nullptr);
        spdlog::debug("DeviceContext: VkDevice destroyed.");
    }

#if VULKAN_VALIDATION
    // VkDebugUtilsMessengerEXT: must be destroyed before the VkInstance it belongs to.
    // The destroy function is a Vulkan extension, loaded via instance proc addr.
    if (_debugMessenger != VK_NULL_HANDLE) {
        auto fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(_instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (fn != nullptr) {
            fn(_instance, _debugMessenger, nullptr);
            spdlog::debug("DeviceContext: VkDebugUtilsMessengerEXT destroyed.");
        }
    }
#endif

    // vkDestroyInstance: releases the Vulkan loader's bookkeeping for this application.
    // Must be the last Vulkan call — nothing can be used after this point.
    if (_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(_instance, nullptr);
        spdlog::debug("DeviceContext: VkInstance destroyed.");
    }
}

// ─── accessors ────────────────────────────────────────────────────────────────

/**
 * @brief Return the VkInstance handle.
 * @return The instance handle, valid for the lifetime of this object.
 */
VkInstance DeviceContext::instance() const noexcept
{
    return _instance;
}

/**
 * @brief Return the selected VkPhysicalDevice.
 * @return The physical device handle, valid for the lifetime of this object.
 */
VkPhysicalDevice DeviceContext::physicalDevice() const noexcept
{
    return _physicalDevice;
}

/**
 * @brief Return the VkDevice (logical device).
 * @return The device handle, valid for the lifetime of this object.
 */
VkDevice DeviceContext::device() const noexcept
{
    return _device;
}

/**
 * @brief Return the combined graphics+present queue handle.
 * @return The queue handle, valid for the lifetime of this object.
 */
VkQueue DeviceContext::graphicsQueue() const noexcept
{
    return _graphicsQueue;
}

/**
 * @brief Return the queue family index for the graphics+present queue.
 * @return The family index used when creating command pools.
 */
uint32_t DeviceContext::graphicsQueueFamily() const noexcept
{
    return _graphicsQueueFamily;
}
