/**
 * @file network/zappy_connection.cpp
 * @brief Implementation of ZappyConnection: StreamPeerTCP polling, WELCOME handshake,
 *        bootstrap queries, and line-buffered parsing via parse_line().
 */

#include "network/zappy_connection.hpp"
#include "network/protocol_parser.hpp"

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace zappy {

void ZappyConnection::connect_to_host(const String& host, int port)
{
    if (_peer.is_valid()) {
        _peer->disconnect_from_host();
    }

    _recvBuf.clear();
    _errorMessage.clear();
    _queue.clear();

    _peer.instantiate();
    const Error err = _peer->connect_to_host(host, port);
    if (err != OK) {
        fail("connect_to_host failed");
        return;
    }

    _state = ConnectionState::Connecting;
}

void ZappyConnection::poll()
{
    if (_state == ConnectionState::Disconnected || _state == ConnectionState::Error) {
        return;
    }

    _peer->poll();

    switch (_peer->get_status()) {
    case StreamPeerTCP::STATUS_CONNECTING:
        return;

    case StreamPeerTCP::STATUS_ERROR:
        fail("TCP connection error");
        return;

    case StreamPeerTCP::STATUS_NONE:
        fail("connection closed by server");
        return;

    case StreamPeerTCP::STATUS_CONNECTED:
        if (_state == ConnectionState::Connecting) {
            _state = ConnectionState::AwaitingWelcome;
        }
        read_available();
        process_lines();
        return;
    }
}

void ZappyConnection::read_available()
{
    const int available = _peer->get_available_bytes();
    if (available <= 0) {
        return;
    }

    const Array result = _peer->get_partial_data(available);
    const Error err    = static_cast<Error>(static_cast<int64_t>(result[0]));
    if (err != OK) {
        fail("get_partial_data failed");
        return;
    }

    const PackedByteArray bytes = result[1];
    const int64_t n             = bytes.size();
    _recvBuf.reserve(_recvBuf.size() + static_cast<std::size_t>(n));
    for (int64_t i = 0; i < n; ++i) {
        _recvBuf.push_back(static_cast<char>(bytes[i]));
    }
}

void ZappyConnection::process_lines()
{
    std::size_t pos = 0;
    while ((pos = _recvBuf.find('\n')) != std::string::npos) {
        std::string line = _recvBuf.substr(0, pos);
        _recvBuf.erase(0, pos + 1);

        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }

        if (_state == ConnectionState::AwaitingWelcome) {
            if (line == "WELCOME") {
                UtilityFunctions::print("ZappyConnection: received WELCOME, sending GRAPHIC + bootstrap");
                send_bootstrap();
                _state = ConnectionState::Ready;
            } else {
                fail("expected WELCOME, got: " + line);
                return;
            }
            continue;
        }

        if (auto msg = parse_line(line)) {
            _queue.push_back(std::move(*msg));
        }
    }
}

void ZappyConnection::send_bootstrap()
{
    send_raw("GRAPHIC\n");
    send_raw("msz\n");
    send_raw("mct\n");
    send_raw("tna\n");
    send_raw("sgt\n");
}

void ZappyConnection::send_raw(std::string_view data)
{
    if (!_peer.is_valid()) {
        return;
    }

    PackedByteArray bytes;
    bytes.resize(static_cast<int64_t>(data.size()));
    for (std::size_t i = 0; i < data.size(); ++i) {
        bytes[static_cast<int64_t>(i)] = static_cast<uint8_t>(data[i]);
    }
    _peer->put_data(bytes);
}

std::vector<ServerMessage> ZappyConnection::drain()
{
    std::vector<ServerMessage> out;
    out.swap(_queue);
    return out;
}

void ZappyConnection::fail(std::string message)
{
    _state        = ConnectionState::Error;
    _errorMessage = std::move(message);
    UtilityFunctions::print("ZappyConnection: error: ", String(_errorMessage.c_str()));
}

} // namespace zappy
