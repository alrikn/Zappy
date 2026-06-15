/**
 * @file network/protocol_parser.hpp
 * @brief Pure-C++ parser turning one protocol line into a ServerMessage.
 * @details No Godot dependencies. ZappyConnection (Phase 3) is the only caller.
 *
 *          Exceptions are disabled project-wide (godot-cpp is built with
 *          -fno-exceptions, which propagates to this target), so parse_line()
 *          cannot throw. Any line with a recognised command but malformed or
 *          missing fields is treated the same as an unrecognised command: it
 *          returns std::nullopt and the caller skips it silently.
 */

#pragma once

#include "network/server_message.hpp"

#include <optional>
#include <string_view>

namespace zappy {

/**
 * @brief Parse one newline-stripped protocol line into a ServerMessage.
 * @param line One complete line from the server, without the trailing '\n'.
 * @return A ServerMessage variant on success, or std::nullopt for an
 *         unrecognised command prefix OR a recognised command with
 *         malformed/missing fields.
 */
std::optional<ServerMessage> parse_line(std::string_view line);

} // namespace zappy
