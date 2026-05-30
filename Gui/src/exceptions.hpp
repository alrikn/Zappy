/**
 * @file exceptions.hpp
 * @brief Base exception hierarchy for the entire zappy_gui application.
 */

#pragma once

#include <stdexcept>
#include <string>

/**
 * @brief Base class for all zappy_gui exceptions.
 * @details Inherits from std::exception. All module-specific exceptions must inherit
 *          from this class rather than throwing std::runtime_error or std::exception
 *          directly. Exception class names must identify their module and failure mode
 *          (e.g. NetworkConnectException, RendererInitException).
 *
 *          Lifetime: thrown and caught — not stored long-term. Constructed with a message
 *          string, destroyed when the exception goes out of the catch scope.
 */
class ZappyException : public std::exception {
public:

    /**
     * @brief Construct with a human-readable error message.
     * @param message Description of the error condition.
     */
    explicit ZappyException(std::string message) : _message(std::move(message)) {}

    /**
     * @brief Return the error message as a null-terminated C string.
     * @return Pointer to the internal message buffer. Valid for the lifetime of this object.
     */
    [[nodiscard]] const char* what() const noexcept override {
        return _message.c_str();
    }

    ~ZappyException() override = default;

protected:
    std::string _message; ///< Human-readable error description.
};
