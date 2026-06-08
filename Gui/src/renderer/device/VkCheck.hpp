/**
 * @file renderer/device/VkCheck.hpp
 * @brief VK_CHECK macro — log and throw on every non-success VkResult.
 * @details Every Vulkan API call that returns a VkResult must be wrapped with
 *          VK_CHECK(). The macro evaluates the expression, and if the result is
 *          anything other than VK_SUCCESS it throws a RendererVkException whose
 *          message includes the expression text and the numeric error code.
 *
 *          This file also declares RendererVkException, but the definition lives in
 *          VkCheck.cpp so that spdlog (and vulkan.h) are not pulled into every
 *          translation unit that includes this header.
 */

#pragma once

#include <vulkan/vulkan.h>
#include "exceptions.hpp"

/**
 * @brief Thrown when a Vulkan API call returns a non-VK_SUCCESS result code.
 * @details Raised exclusively by the VK_CHECK() macro. The message includes both
 *          the VkResult integer value and the expression text that failed, making
 *          it straightforward to locate the failing call in a log.
 *
 *          Declared here (rather than in exceptions.hpp) so that vulkan.h is not
 *          transitively pulled into every translation unit that includes exceptions.hpp.
 *          Only renderer translation units include VkCheck.hpp.
 *
 *          Lifetime: thrown from any point inside the renderer where VK_CHECK is used,
 *          propagates up to main()'s top-level catch.
 */
class RendererVkException : public ZappyException {
public:
    /**
     * @brief Construct with the failing VkResult code and the expression string.
     * @param result   The VkResult value that was not VK_SUCCESS.
     * @param callSite String literal of the expression that was checked (from #expr in the macro).
     */
    RendererVkException(VkResult result, const char* callSite);
};

/**
 * @brief Evaluate expr (must produce a VkResult), throw RendererVkException on failure.
 * @details Used at every Vulkan call site in the renderer. The macro preserves the
 *          expression text via the preprocessor stringification operator (#expr), so
 *          the exception message contains the exact source code that failed.
 *
 *          The do { ... } while(0) idiom makes the macro safe to use in any context
 *          that expects a single statement (e.g. inside an if without braces), without
 *          introducing a dangling-else hazard or requiring a semicolon after the macro.
 *
 *          Example:
 *          @code
 *          VK_CHECK(vkCreateRenderPass(device, &info, nullptr, &_renderPass));
 *          @endcode
 */
#define VK_CHECK(expr)                                          \
    do {                                                        \
        VkResult _vkr = (expr);                                 \
        if (_vkr != VK_SUCCESS) {                               \
            throw RendererVkException(_vkr, #expr);             \
        }                                                       \
    } while (0)
