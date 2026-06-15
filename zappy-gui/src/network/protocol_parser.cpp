/**
 * @file network/protocol_parser.cpp
 * @brief Implementation of parse_line(): dispatches on the first token of a protocol
 *        line and fills the matching ServerMessage alternative.
 * @details Exception-free (see protocol_parser.hpp): every failure path returns
 *          std::nullopt instead of throwing.
 */

#include "network/protocol_parser.hpp"

#include <array>
#include <charconv>
#include <cstdint>
#include <sstream>
#include <string>

namespace zappy {

namespace {

/**
 * @brief Parse a player/egg ID token that the protocol prefixes with '#'.
 * @param token The raw token string, e.g. "#5" or "#42".
 * @return Parsed numeric ID, or std::nullopt if the token is not '#'-prefixed
 *         or not entirely numeric after the '#'.
 */
std::optional<uint32_t> read_id(std::string_view token)
{
    if (token.empty() || token[0] != '#') {
        return std::nullopt;
    }
    const std::string_view digits = token.substr(1);
    if (digits.empty()) {
        return std::nullopt;
    }
    uint32_t value = 0;
    const auto* begin  = digits.data();
    const auto* end    = digits.data() + digits.size();
    const auto  result = std::from_chars(begin, end, value);
    if (result.ec != std::errc{} || result.ptr != end) {
        return std::nullopt;
    }
    return value;
}

/**
 * @brief Read a '#'-prefixed ID token from a stream.
 * @return Parsed numeric ID, or std::nullopt if the stream has no more tokens or
 *         the token is malformed.
 */
std::optional<uint32_t> read_id_from_stream(std::istringstream& ss)
{
    std::string tok;
    if (!(ss >> tok)) {
        return std::nullopt;
    }
    return read_id(tok);
}

/**
 * @brief Read a 7-element resource array from a stream.
 * @param ss Input stream positioned just before the first resource quantity.
 * @param arr Output array to fill.
 * @return true if all 7 values were read successfully.
 */
bool read_resources(std::istringstream& ss, std::array<uint32_t, 7>& arr)
{
    for (auto& slot : arr) {
        if (!(ss >> slot)) {
            return false;
        }
    }
    return true;
}

} // namespace

std::optional<ServerMessage> parse_line(std::string_view line)
{
    std::string        lineStr(line);
    std::istringstream ss(lineStr);
    std::string        cmd;

    if (!(ss >> cmd)) {
        return std::nullopt;
    }

    // ── Map size ─────────────────────────────────────────────────────────────
    if (cmd == "msz") {
        MsgMapSize m{};
        if (!(ss >> m.x >> m.y)) {
            return std::nullopt;
        }
        return m;
    }

    // ── Tile content ─────────────────────────────────────────────────────────
    if (cmd == "bct") {
        MsgTileContent m{};
        if (!(ss >> m.x >> m.y) || !read_resources(ss, m.resources)) {
            return std::nullopt;
        }
        return m;
    }

    // ── Team name ────────────────────────────────────────────────────────────
    if (cmd == "tna") {
        MsgTeamName m{};
        if (!(ss >> m.name)) {
            return std::nullopt;
        }
        return m;
    }

    // ── New player ───────────────────────────────────────────────────────────
    if (cmd == "pnw") {
        MsgPlayerNew m{};
        const auto id = read_id_from_stream(ss);
        uint32_t   orientation = 0;
        uint32_t   level       = 0;
        if (!id || !(ss >> m.x >> m.y >> orientation >> level >> m.team)) {
            return std::nullopt;
        }
        m.id          = *id;
        m.orientation = static_cast<uint8_t>(orientation);
        m.level       = static_cast<uint8_t>(level);
        return m;
    }

    // ── Player position ──────────────────────────────────────────────────────
    if (cmd == "ppo") {
        MsgPlayerPosition m{};
        const auto id = read_id_from_stream(ss);
        uint32_t   orientation = 0;
        if (!id || !(ss >> m.x >> m.y >> orientation)) {
            return std::nullopt;
        }
        m.id          = *id;
        m.orientation = static_cast<uint8_t>(orientation);
        return m;
    }

    // ── Player level ─────────────────────────────────────────────────────────
    if (cmd == "plv") {
        MsgPlayerLevel m{};
        const auto id    = read_id_from_stream(ss);
        uint32_t   level = 0;
        if (!id || !(ss >> level)) {
            return std::nullopt;
        }
        m.id    = *id;
        m.level = static_cast<uint8_t>(level);
        return m;
    }

    // ── Player inventory ─────────────────────────────────────────────────────
    if (cmd == "pin") {
        MsgPlayerInventory m{};
        const auto id = read_id_from_stream(ss);
        if (!id || !(ss >> m.x >> m.y) || !read_resources(ss, m.resources)) {
            return std::nullopt;
        }
        m.id = *id;
        return m;
    }

    // ── Player expulsion ─────────────────────────────────────────────────────
    if (cmd == "pex") {
        MsgPlayerExpulsion m{};
        const auto id = read_id_from_stream(ss);
        if (!id) {
            return std::nullopt;
        }
        m.id = *id;
        return m;
    }

    // ── Player broadcast ─────────────────────────────────────────────────────
    if (cmd == "pbc") {
        MsgPlayerBroadcast m{};
        const auto id = read_id_from_stream(ss);
        if (!id) {
            return std::nullopt;
        }
        m.id = *id;
        // The message text may contain spaces; std::getline reads the rest of the line.
        // We skip any leading whitespace between the ID and the message with std::ws.
        std::getline(ss >> std::ws, m.message);
        return m;
    }

    // ── Player death ─────────────────────────────────────────────────────────
    if (cmd == "pdi") {
        MsgPlayerDeath m{};
        const auto id = read_id_from_stream(ss);
        if (!id) {
            return std::nullopt;
        }
        m.id = *id;
        return m;
    }

    // ── Incantation start ────────────────────────────────────────────────────
    if (cmd == "pic") {
        MsgIncantationStart m{};
        uint32_t level = 0;
        if (!(ss >> m.x >> m.y >> level)) {
            return std::nullopt;
        }
        m.level = static_cast<uint8_t>(level);
        // Read variable number of participating player IDs.
        std::string tok;
        while (ss >> tok) {
            const auto id = read_id(tok);
            if (!id) {
                return std::nullopt;
            }
            m.players.push_back(*id);
        }
        if (m.players.empty()) {
            return std::nullopt;
        }
        return m;
    }

    // ── Incantation end ──────────────────────────────────────────────────────
    if (cmd == "pie") {
        MsgIncantationEnd m{};
        std::string result;
        if (!(ss >> m.x >> m.y >> result)) {
            return std::nullopt;
        }
        if (result == "ok") {
            m.result = true;
        } else if (result == "ko") {
            m.result = false;
        } else {
            return std::nullopt;
        }
        return m;
    }

    // ── Egg laying start ─────────────────────────────────────────────────────
    if (cmd == "pfk") {
        MsgEggLaying m{};
        const auto id = read_id_from_stream(ss);
        if (!id) {
            return std::nullopt;
        }
        m.playerId = *id;
        return m;
    }

    // ── Resource drop ────────────────────────────────────────────────────────
    if (cmd == "pdr") {
        MsgResourceDrop m{};
        const auto id  = read_id_from_stream(ss);
        uint32_t   res = 0;
        if (!id || !(ss >> res)) {
            return std::nullopt;
        }
        m.playerId = *id;
        m.resource = static_cast<uint8_t>(res);
        return m;
    }

    // ── Resource collect ─────────────────────────────────────────────────────
    if (cmd == "pgt") {
        MsgResourceCollect m{};
        const auto id  = read_id_from_stream(ss);
        uint32_t   res = 0;
        if (!id || !(ss >> res)) {
            return std::nullopt;
        }
        m.playerId = *id;
        m.resource = static_cast<uint8_t>(res);
        return m;
    }

    // ── Egg laid at position ─────────────────────────────────────────────────
    if (cmd == "enw") {
        MsgEggLaid m{};
        const auto eggId    = read_id_from_stream(ss);
        const auto playerId = read_id_from_stream(ss);
        if (!eggId || !playerId || !(ss >> m.x >> m.y)) {
            return std::nullopt;
        }
        m.eggId    = *eggId;
        m.playerId = *playerId;
        return m;
    }

    // ── Player connected via egg ─────────────────────────────────────────────
    if (cmd == "ebo") {
        MsgEggConnection m{};
        const auto eggId = read_id_from_stream(ss);
        if (!eggId) {
            return std::nullopt;
        }
        m.eggId = *eggId;
        return m;
    }

    // ── Egg death ────────────────────────────────────────────────────────────
    if (cmd == "edi") {
        MsgEggDeath m{};
        const auto eggId = read_id_from_stream(ss);
        if (!eggId) {
            return std::nullopt;
        }
        m.eggId = *eggId;
        return m;
    }

    // ── Time unit query response ─────────────────────────────────────────────
    if (cmd == "sgt") {
        MsgTimeUnit m{};
        if (!(ss >> m.t)) {
            return std::nullopt;
        }
        return m;
    }

    // ── Time unit changed ────────────────────────────────────────────────────
    if (cmd == "sst") {
        MsgTimeUnitSet m{};
        if (!(ss >> m.t)) {
            return std::nullopt;
        }
        return m;
    }

    // ── End of game ──────────────────────────────────────────────────────────
    if (cmd == "seg") {
        MsgEndGame m{};
        if (!(ss >> m.team)) {
            return std::nullopt;
        }
        return m;
    }

    // ── Server message ───────────────────────────────────────────────────────
    if (cmd == "smg") {
        MsgServerMessage m{};
        // Free-form text; may contain spaces. Read the rest of the line.
        std::getline(ss >> std::ws, m.message);
        return m;
    }

    // ── Unknown command ──────────────────────────────────────────────────────
    if (cmd == "suc") {
        return MsgUnknownCommand{};
    }

    // ── Bad parameter ────────────────────────────────────────────────────────
    if (cmd == "sbp") {
        return MsgBadParameter{};
    }

    // Unrecognised command prefix: ignore silently. This layer has no Godot deps
    // and therefore no logging facility; ZappyConnection may log if it wants to.
    return std::nullopt;
}

} // namespace zappy
