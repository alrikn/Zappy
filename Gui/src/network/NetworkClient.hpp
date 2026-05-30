/**
 * @file NetworkClient.hpp
 * @brief TCP connection to the zappy_server.
 * @details Responsibility: own a single non-blocking TCP socket, buffer raw bytes
 *          received from the server, and expose parsed protocol messages to the rest
 *          of the application.
 *
 *          Placeholder until the network feature is implemented. The class exists so
 *          main.cpp compiles; the actual socket logic is added in the network feature.
 *
 *          Architecture position:
 *          main() creates one NetworkClient with the host/port from argv.
 *          A dedicated network thread (added in the network feature) calls recv() and
 *          pushes parsed messages into a thread-safe queue consumed by WorldState.
 */

#pragma once

#include <cstdint>
#include <string>

/**
 * @brief Owns the TCP connection to the zappy_server.
 * @details Lifetime: created once in main(), destroyed when main() returns.
 *          Non-copyable by design: there is exactly one server connection per process.
 *          A std::unique_ptr<NetworkClient> in main() enforces this.
 */
class NetworkClient {
public:
    /**
     * @brief Construct with the server address and port parsed from argv.
     * @details Does NOT open the socket yet — connection happens in connect() (stub).
     * @param host Hostname or IP address of the server (e.g. "localhost").
     * @param port TCP port the server is listening on (e.g. 4242).
     */
    NetworkClient(std::string host, uint16_t port);

    /**
     * @brief Destructor closes the socket if open (stub: no-op for now).
     * @details Default is sufficient because we store no raw resources yet.
     */
    ~NetworkClient() = default;

    NetworkClient(const NetworkClient&) = delete;             ///< Non-copyable: one connection per process.
    NetworkClient& operator=(const NetworkClient&) = delete;  ///< Non-copyable: one connection per process.

    /**
     * @brief Movable: allows the object to be transferred between scopes if needed.
     * @details E.g. moving into a thread wrapper. Not needed yet — stub only.
     */
    NetworkClient(NetworkClient&&) = default;
    NetworkClient& operator=(NetworkClient&&) = default;

private:
    std::string _host;  ///< Server hostname or IP
    uint16_t    _port;  ///< Server TCP port
};

inline NetworkClient::NetworkClient(std::string host, uint16_t port)
    : _host(std::move(host)), _port(port)
{
    // std::move(host): transfers ownership of the string's heap buffer into
    // _host without copying it. This is the idiomatic way to take a string
    // by value and store it efficiently — the parameter acts as a sink.
}
