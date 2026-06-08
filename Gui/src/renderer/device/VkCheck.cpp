/**
 * @file renderer/device/VkCheck.cpp
 * @brief Implementation of RendererVkException constructor.
 * @details Formats the VkResult integer and the failing expression text into a
 *          human-readable message. spdlog is used to log the error immediately
 *          at construction so the message appears in the log even if the exception
 *          is caught and swallowed higher up.
 *
 *          Architecture: included by the linker once; VkCheck.hpp is the header
 *          that every renderer translation unit includes to access VK_CHECK.
 */

#include "renderer/device/VkCheck.hpp"
#include <spdlog/spdlog.h>

/**
 * @brief Construct with the failing VkResult code and the source expression string.
 * @param result   The VkResult value that was not VK_SUCCESS.
 * @param callSite String literal of the expression that was checked (from #expr in the macro).
 */
RendererVkException::RendererVkException(VkResult result, const char* callSite)
    : ZappyException(
        std::string("VK_CHECK failed [") +
        std::to_string(static_cast<int>(result)) +
        "]: " +
        callSite)
{
    // Log immediately so the error appears in the spdlog output even if the
    // exception is caught somewhere and its message is not re-logged.
    spdlog::error("RendererVkException: VkResult={} expression='{}'",
                  static_cast<int>(result), callSite);
}
