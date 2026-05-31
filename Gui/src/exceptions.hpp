/**
 * @file exceptions.hpp
 * @brief Base exception hierarchy for the entire zappy_gui application.
 * @details Every module-specific exception class is defined here, inheriting from
 *          ZappyException. The top-level try/catch in main() catches ZappyException
 *          and logs its message before returning EXIT_FAILURE, giving every module a
 *          single mechanism to signal fatal errors cleanly.
 */

#pragma once

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

/**
 * @brief Thrown when the TCP connection to the server cannot be established.
 * @details Covers: getaddrinfo failure, socket() failure, connect() failure, or the
 *          WELCOME handshake not completing as expected. Thrown from
 *          NetworkClient::connect() and NetworkClient::sendRaw().
 *
 *          Lifetime: thrown from the network layer, caught in main() or a caller that
 *          wants to present an error message before exiting.
 */
class NetworkConnectException : public ZappyException {
public:
    /**
     * @brief Construct with a description of the connection failure.
     * @param message Human-readable explanation (e.g. "getaddrinfo failed for localhost:4242").
     */
    explicit NetworkConnectException(std::string message)
        : ZappyException(std::move(message)) {}
};

/**
 * @brief Thrown when the background recv thread detects an unexpected disconnect.
 * @details The recv thread cannot throw across thread boundaries (there is no caller
 *          frame to unwind into). Instead it sets an atomic flag. NetworkClient::poll()
 *          checks the flag and throws this exception on the main thread, where it can
 *          be caught normally.
 *
 *          Lifetime: thrown from NetworkClient::poll(), caught in the main event loop.
 */
class NetworkRecvException : public ZappyException {
public:
    /**
     * @brief Construct with a description of the recv failure.
     * @param message Human-readable explanation (e.g. "server disconnected unexpectedly").
     */
    explicit NetworkRecvException(std::string message)
        : ZappyException(std::move(message)) {}
};

/**
 * @brief Thrown when a server message line cannot be parsed according to the protocol.
 * @details Thrown from NetworkClient::parseLine() for known command prefixes whose
 *          fields are malformed or missing. Caught inside recvLoop() — a parse error
 *          on one line is logged and skipped, not fatal to the connection.
 *
 *          Lifetime: thrown and caught within recvLoop(); never propagates to main().
 */
class NetworkParseException : public ZappyException {
public:
    /**
     * @brief Construct with a description of the parse failure.
     * @param message Human-readable explanation including the offending line text.
     */
    explicit NetworkParseException(std::string message)
        : ZappyException(std::move(message)) {}
};

/**
 * @brief Thrown when glfwInit() fails.
 * @details glfwInit() can fail if no display is available (e.g. running without a
 *          DISPLAY environment variable on X11, or without a compositor on Wayland).
 *
 *          Lifetime: thrown from main(), caught by the top-level try/catch.
 */
class GlfwInitException : public ZappyException {
public:
    /**
     * @brief Construct with a description of the GLFW initialisation failure.
     * @param message Human-readable explanation.
     */
    explicit GlfwInitException(std::string message)
        : ZappyException(std::move(message)) {}
};

/**
 * @brief Thrown when glfwCreateWindow() fails.
 * @details Can fail if the windowing system rejects the window parameters, or if
 *          glfwInit() was not called first (programming error).
 *
 *          Lifetime: thrown from main(), caught by the top-level try/catch.
 */
class GlfwWindowException : public ZappyException {
public:
    /**
     * @brief Construct with a description of the window creation failure.
     * @param message Human-readable explanation.
     */
    explicit GlfwWindowException(std::string message)
        : ZappyException(std::move(message)) {}
};

/**
 * @brief Thrown when WorldState encounters an irrecoverable structural inconsistency.
 * @details This exception signals programming errors rather than normal network noise.
 *          Example: calling snapshot() before any msz message has been received in a
 *          context where the caller requires a non-empty grid.
 *
 *          Note: WorldState::apply() never throws this — it uses log-and-ignore for
 *          invalid protocol messages. This class exists for callers that explicitly
 *          validate world state before using it.
 *
 *          Lifetime: thrown from world-state validation code, caught by the caller or
 *          the top-level try/catch in main().
 */
class WorldStateException : public ZappyException {
public:
    /**
     * @brief Construct with a description of the inconsistency.
     * @param message Human-readable explanation of why the state is invalid.
     */
    explicit WorldStateException(std::string message)
        : ZappyException(std::move(message)) {}
};
