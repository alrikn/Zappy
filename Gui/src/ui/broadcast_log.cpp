/**
 * @file ui/broadcast_log.cpp
 * @brief Implementation of BroadcastLog.
 */

#include "ui/broadcast_log.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/node_path.hpp>

#include <cctype>
#include <optional>
#include <string>

using namespace godot;

namespace {

// AI bots hex-encode broadcast payloads (see Ai/heuristic_ai/ai_comms.py::_bcast)
// since the server protocol only accepts a single whitespace-free token. Decode
// that for the log so it reads as text instead of a wall of hex digits.
std::optional<std::string> try_hex_decode(const std::string& text)
{
    if (text.empty() || text.size() % 2 != 0) {
        return std::nullopt;
    }
    std::string decoded;
    decoded.reserve(text.size() / 2);
    for (size_t i = 0; i < text.size(); i += 2) {
        unsigned char hi = static_cast<unsigned char>(text[i]);
        unsigned char lo = static_cast<unsigned char>(text[i + 1]);
        if (!std::isxdigit(hi) || !std::isxdigit(lo)) {
            return std::nullopt;
        }
        auto nibble = [](unsigned char c) { return c <= '9' ? c - '0' : (std::tolower(c) - 'a' + 10); };
        decoded.push_back(static_cast<char>((nibble(hi) << 4) | nibble(lo)));
    }
    return decoded;
}

// Number of UTF-8 continuation bytes following a leading byte, or -1 if it
// isn't a valid leading byte.
int utf8_extra_bytes(unsigned char c)
{
    if ((c & 0x80) == 0x00) return 0;
    if ((c & 0xE0) == 0xC0) return 1;
    if ((c & 0xF0) == 0xE0) return 2;
    if ((c & 0xF8) == 0xF0) return 3;
    return -1;
}

// Reject decodes that aren't valid UTF-8 so plaintext broadcasts that happen to
// look like hex (e.g. "deadbeef") aren't mangled into garbage.
bool is_valid_utf8(const std::string& s)
{
    size_t i = 0;
    while (i < s.size()) {
        int extra = utf8_extra_bytes(static_cast<unsigned char>(s[i]));
        if (extra < 0) return false;
        if (i + static_cast<size_t>(extra) >= s.size()) return false;
        for (size_t j = 1; j <= static_cast<size_t>(extra); j++) {
            if ((static_cast<unsigned char>(s[i + j]) & 0xC0) != 0x80) return false;
        }
        i += static_cast<size_t>(extra) + 1;
    }
    return true;
}

} // namespace

void BroadcastLog::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("on_broadcast", "id", "message"), &BroadcastLog::on_broadcast);
    ClassDB::bind_method(D_METHOD("on_server_message", "message"), &BroadcastLog::on_server_message);
}

void BroadcastLog::_ready()
{
    _log = get_node<RichTextLabel>(NodePath("Panel/LogText"));
    _log->set_scroll_follow(true);
}

void BroadcastLog::on_broadcast(int id, const String& message)
{
    std::string raw(message.utf8().get_data());
    std::optional<std::string> decoded = try_hex_decode(raw);
    String display = (decoded && is_valid_utf8(*decoded)) ? String::utf8(decoded->c_str()) : message;

    // add_text(), not append_text(): the message is untrusted player input
    // and must not be interpreted as BBCode.
    _log->add_text("[Player " + String::num_int64(id) + "] " + display + "\n");
}

void BroadcastLog::on_server_message(const String& message)
{
    _log->add_text("[Server] " + message + "\n");
}
