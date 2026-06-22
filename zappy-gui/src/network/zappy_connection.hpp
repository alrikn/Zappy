/**
 * @file network/zappy_connection.hpp
 * @brief TCP connection to zappy_server as a GRAPHIC client, via godot-cpp's StreamPeerTCP.
 * @details Plain C++ class (not a GDCLASS) wrapping Ref<StreamPeerTCP>. Single-threaded:
 *          ZappyWorld::_process() calls poll() once per frame, then drain() to retrieve
 *          every ServerMessage parsed since the last call.
 *
 *          State machine: Disconnected -> Connecting -> AwaitingWelcome -> Ready, with an
 *          Error state (carrying a message) reachable from any state on TCP failure.
 */

#pragma once

#include "network/server_message.hpp"

#include <godot_cpp/classes/stream_peer_tcp.hpp>
#include <godot_cpp/variant/string.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace zappy {

/// Connection lifecycle state. See zappy_connection.hpp for transitions.
enum class ConnectionState {
    Disconnected,   ///< Initial state; no peer created yet.
    Connecting,     ///< TCP handshake in progress (StreamPeerTCP::STATUS_CONNECTING).
    AwaitingWelcome,///< TCP connected; waiting for the server's "WELCOME" line.
    Ready,          ///< Handshake complete; bootstrap sent; messages are being parsed.
    Error,          ///< Connection failed or was lost; see error_message().
};

/**
 * @brief Owns the TCP connection to zappy_server and the bootstrap/parse pipeline.
 * @details Lifetime: owned as a plain member of ZappyWorld. Not copyable/movable —
 *          there is exactly one connection per ZappyWorld.
 */
class ZappyConnection {
public:
    ZappyConnection() = default;
    ~ZappyConnection() = default;

    ZappyConnection(const ZappyConnection&)            = delete;
    ZappyConnection& operator=(const ZappyConnection&) = delete;
    ZappyConnection(ZappyConnection&&)                 = delete;
    ZappyConnection& operator=(ZappyConnection&&)      = delete;

    /**
     * @brief Start connecting to the given host/port.
     * @details Disconnects any previous peer, creates a fresh StreamPeerTCP, and issues
     *          connect_to_host(). Resets the receive buffer, message queue and error state.
     *          Transitions to Connecting on success, or Error if connect_to_host() fails
     *          immediately (e.g. invalid host).
     */
    void connect_to_host(const godot::String& host, int port);

    /**
     * @brief Advance the connection state machine. Call once per ZappyWorld::_process().
     * @details Polls the underlying peer, reacts to status changes, reads any available
     *          bytes, and parses complete lines into the internal queue.
     */
    void poll();

    /**
     * @brief Return and clear all messages parsed since the last call.
     */
    [[nodiscard]] std::vector<ServerMessage> drain();

    /**
     * @brief Send raw bytes to the server (e.g. "ppo\n" style commands).
     */
    void send_raw(std::string_view data);

    /// True once the connection has failed or been lost. See error_message().
    [[nodiscard]] bool has_error() const noexcept { return _state == ConnectionState::Error; }

    /// True once the WELCOME/GRAPHIC/bootstrap handshake is complete.
    [[nodiscard]] bool is_ready() const noexcept { return _state == ConnectionState::Ready; }

    /// Human-readable description of the last error. Empty if has_error() is false.
    [[nodiscard]] const std::string& error_message() const noexcept { return _errorMessage; }

private:
    /// Pull any bytes currently available on the socket into _recvBuf.
    void read_available();

    /// Extract and handle every complete '\n'-terminated line in _recvBuf.
    void process_lines();

    /// Send GRAPHIC plus the msz/mct/tna/sgt bootstrap queries.
    void send_bootstrap();

    /// Move to the Error state and record a message; further poll() calls are no-ops.
    void fail(std::string message);

    godot::Ref<godot::StreamPeerTCP> _peer;
    ConnectionState _state{ConnectionState::Disconnected};
    std::string _recvBuf;
    std::string _errorMessage;
    std::vector<ServerMessage> _queue;
};

} // namespace zappy
