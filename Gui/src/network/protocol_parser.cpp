/**
 * @file network/protocol_parser.cpp
 * @brief Implementation of parse_line(): dispatches on the first token of a protocol
 *        line and fills the matching ServerMessage alternative.
 * @details Exception-free (see protocol_parser.hpp): every failure path returns
 *          std::nullopt instead of throwing.
 *
 *          Tokenizing and numeric parsing deliberately avoid std::istringstream's
 *          operator>>: in this GDExtension, that path was observed to silently
 *          mis-parse digits (e.g. "40" read back as 0) specifically when running
 *          inside a windowed Godot process with its rendering/audio/shader threads
 *          active, while always parsing correctly in --headless runs. Tokenizing by
 *          hand and converting digits over string_view indices (no streams, no
 *          locale) sidesteps whatever that interaction is.
 */

#include "network/protocol_parser.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <string>

namespace zappy {

namespace {

/// True for the byte separators used between protocol tokens.
constexpr bool is_space(char c) noexcept
{
    return c == ' ' || c == '\t';
}

/**
 * @brief Hand-rolled whitespace tokenizer over a single protocol line.
 * @details Replaces std::istringstream::operator>> for splitting; see the file-level
 *          comment for why.
 */
class Tokenizer {
public:
    explicit Tokenizer(std::string_view text) noexcept : _rest(text) {}

    /// Return the next whitespace-delimited token, or std::nullopt if exhausted.
    std::optional<std::string_view> next() noexcept
    {
        std::size_t i = 0;
        while (i < _rest.size() && is_space(_rest[i])) {
            ++i;
        }
        if (i >= _rest.size()) {
            _rest = {};
            return std::nullopt;
        }
        std::size_t j = i;
        while (j < _rest.size() && !is_space(_rest[j])) {
            ++j;
        }
        const std::string_view tok = _rest.substr(i, j - i);
        _rest.remove_prefix(j);
        return tok;
    }

    /// Everything after the last token returned by next(), with leading whitespace
    /// trimmed but internal whitespace preserved. Used for free-form trailing text
    /// (broadcast/server messages) that may itself contain spaces.
    [[nodiscard]] std::string_view remainder_trimmed() const noexcept
    {
        std::size_t i = 0;
        while (i < _rest.size() && is_space(_rest[i])) {
            ++i;
        }
        return _rest.substr(i);
    }

private:
    std::string_view _rest;
};

/**
 * @brief Parse a whole token as an unsigned integer.
 * @details Requires every character to be an ASCII digit and the accumulated value
 *          to fit in T; otherwise returns std::nullopt. Pure index-based scanning
 *          over the string_view — no pointers, no locale, no streams.
 */
template<typename T>
std::optional<T> parse_uint(std::string_view token) noexcept
{
    if (token.empty()) {
        return std::nullopt;
    }
    T value{};
    for (std::size_t i = 0; i < token.size(); ++i) {
        const char c = token[i];
        if (c < '0' || c > '9') {
            return std::nullopt;
        }
        const T digit = static_cast<T>(c - '0');
        if (value > (std::numeric_limits<T>::max() - digit) / 10) {
            return std::nullopt; // would overflow T
        }
        value = static_cast<T>(value * 10 + digit);
    }
    return value;
}

/// Read the next token from `tok` and parse it as an unsigned integer.
template<typename T>
std::optional<T> next_uint(Tokenizer& tok) noexcept
{
    const auto token = tok.next();
    if (!token) {
        return std::nullopt;
    }
    return parse_uint<T>(*token);
}

/**
 * @brief Parse a player/egg ID token that the protocol prefixes with '#'.
 * @param token The raw token string, e.g. "#5" or "#42".
 * @return Parsed numeric ID, or std::nullopt if the token is not '#'-prefixed
 *         or not entirely numeric after the '#'.
 */
std::optional<uint32_t> read_id(std::string_view token) noexcept
{
    if (token.empty() || token[0] != '#') {
        return std::nullopt;
    }
    return parse_uint<uint32_t>(token.substr(1));
}

/// Read the next '#'-prefixed ID token from `tok`.
std::optional<uint32_t> next_id(Tokenizer& tok) noexcept
{
    const auto token = tok.next();
    if (!token) {
        return std::nullopt;
    }
    return read_id(*token);
}

/**
 * @brief Read a 7-element resource array from `tok`.
 * @param tok Tokenizer positioned just before the first resource quantity.
 * @param arr Output array to fill.
 * @return true if all 7 values were read successfully.
 */
bool read_resources(Tokenizer& tok, std::array<uint32_t, 7>& arr) noexcept
{
    for (auto& slot : arr) {
        const auto value = next_uint<uint32_t>(tok);
        if (!value) {
            return false;
        }
        slot = *value;
    }
    return true;
}

} // namespace

std::optional<ServerMessage> parse_line(std::string_view line)
{
    Tokenizer tok(line);

    const auto cmdTok = tok.next();
    if (!cmdTok) {
        return std::nullopt;
    }
    const std::string_view cmd = *cmdTok;

    // ── Map size ─────────────────────────────────────────────────────────────
    if (cmd == "msz") {
        MsgMapSize m{};
        const auto x = next_uint<uint32_t>(tok);
        const auto y = next_uint<uint32_t>(tok);
        if (!x || !y) {
            return std::nullopt;
        }
        m.x = *x;
        m.y = *y;
        return m;
    }

    // ── Tile content ─────────────────────────────────────────────────────────
    if (cmd == "bct") {
        MsgTileContent m{};
        const auto x = next_uint<uint32_t>(tok);
        const auto y = next_uint<uint32_t>(tok);
        if (!x || !y || !read_resources(tok, m.resources)) {
            return std::nullopt;
        }
        m.x = *x;
        m.y = *y;
        return m;
    }

    // ── Team name ────────────────────────────────────────────────────────────
    if (cmd == "tna") {
        MsgTeamName m{};
        const auto name = tok.next();
        if (!name) {
            return std::nullopt;
        }
        m.name = std::string(*name);
        return m;
    }

    // ── New player ───────────────────────────────────────────────────────────
    if (cmd == "pnw") {
        MsgPlayerNew m{};
        const auto id          = next_id(tok);
        const auto x           = next_uint<uint32_t>(tok);
        const auto y           = next_uint<uint32_t>(tok);
        const auto orientation = next_uint<uint32_t>(tok);
        const auto level       = next_uint<uint32_t>(tok);
        const auto team        = tok.next();
        if (!id || !x || !y || !orientation || !level || !team) {
            return std::nullopt;
        }
        m.id          = *id;
        m.x           = *x;
        m.y           = *y;
        m.orientation = static_cast<uint8_t>(*orientation);
        m.level       = static_cast<uint8_t>(*level);
        m.team        = std::string(*team);
        return m;
    }

    // ── Player position ──────────────────────────────────────────────────────
    if (cmd == "ppo") {
        MsgPlayerPosition m{};
        const auto id          = next_id(tok);
        const auto x           = next_uint<uint32_t>(tok);
        const auto y           = next_uint<uint32_t>(tok);
        const auto orientation = next_uint<uint32_t>(tok);
        if (!id || !x || !y || !orientation) {
            return std::nullopt;
        }
        m.id          = *id;
        m.x           = *x;
        m.y           = *y;
        m.orientation = static_cast<uint8_t>(*orientation);
        return m;
    }

    // ── Player level ─────────────────────────────────────────────────────────
    if (cmd == "plv") {
        MsgPlayerLevel m{};
        const auto id    = next_id(tok);
        const auto level = next_uint<uint32_t>(tok);
        if (!id || !level) {
            return std::nullopt;
        }
        m.id    = *id;
        m.level = static_cast<uint8_t>(*level);
        return m;
    }

    // ── Player inventory ─────────────────────────────────────────────────────
    if (cmd == "pin") {
        MsgPlayerInventory m{};
        const auto id = next_id(tok);
        const auto x  = next_uint<uint32_t>(tok);
        const auto y  = next_uint<uint32_t>(tok);
        if (!id || !x || !y || !read_resources(tok, m.resources)) {
            return std::nullopt;
        }
        m.id = *id;
        m.x  = *x;
        m.y  = *y;
        return m;
    }

    // ── Player expulsion ─────────────────────────────────────────────────────
    if (cmd == "pex") {
        MsgPlayerExpulsion m{};
        const auto id = next_id(tok);
        if (!id) {
            return std::nullopt;
        }
        m.id = *id;
        return m;
    }

    // ── Player broadcast ─────────────────────────────────────────────────────
    if (cmd == "pbc") {
        MsgPlayerBroadcast m{};
        const auto id = next_id(tok);
        if (!id) {
            return std::nullopt;
        }
        m.id = *id;
        // The message text may contain spaces; everything after the id token is the
        // message, leading whitespace trimmed.
        m.message = std::string(tok.remainder_trimmed());
        return m;
    }

    // ── Player death ─────────────────────────────────────────────────────────
    if (cmd == "pdi") {
        MsgPlayerDeath m{};
        const auto id = next_id(tok);
        if (!id) {
            return std::nullopt;
        }
        m.id = *id;
        return m;
    }

    // ── Incantation start ────────────────────────────────────────────────────
    if (cmd == "pic") {
        MsgIncantationStart m{};
        const auto x     = next_uint<uint32_t>(tok);
        const auto y     = next_uint<uint32_t>(tok);
        const auto level = next_uint<uint32_t>(tok);
        if (!x || !y || !level) {
            return std::nullopt;
        }
        m.x     = *x;
        m.y     = *y;
        m.level = static_cast<uint8_t>(*level);
        // Read variable number of participating player IDs.
        while (const auto token = tok.next()) {
            const auto id = read_id(*token);
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
        const auto x      = next_uint<uint32_t>(tok);
        const auto y      = next_uint<uint32_t>(tok);
        const auto result = tok.next();
        if (!x || !y || !result) {
            return std::nullopt;
        }
        m.x = *x;
        m.y = *y;
        if (*result == "1") {
            m.result = true;
        } else if (*result == "0") {
            m.result = false;
        } else {
            return std::nullopt;
        }
        return m;
    }

    // ── Egg laying start ─────────────────────────────────────────────────────
    if (cmd == "pfk") {
        MsgEggLaying m{};
        const auto id = next_id(tok);
        if (!id) {
            return std::nullopt;
        }
        m.playerId = *id;
        return m;
    }

    // ── Resource drop ────────────────────────────────────────────────────────
    if (cmd == "pdr") {
        MsgResourceDrop m{};
        const auto id  = next_id(tok);
        const auto res = next_uint<uint32_t>(tok);
        if (!id || !res) {
            return std::nullopt;
        }
        m.playerId = *id;
        m.resource = static_cast<uint8_t>(*res);
        return m;
    }

    // ── Resource collect ─────────────────────────────────────────────────────
    if (cmd == "pgt") {
        MsgResourceCollect m{};
        const auto id  = next_id(tok);
        const auto res = next_uint<uint32_t>(tok);
        if (!id || !res) {
            return std::nullopt;
        }
        m.playerId = *id;
        m.resource = static_cast<uint8_t>(*res);
        return m;
    }

    // ── Egg laid at position ─────────────────────────────────────────────────
    if (cmd == "enw") {
        MsgEggLaid m{};
        const auto eggId    = next_id(tok);
        const auto playerId = next_id(tok);
        const auto x        = next_uint<uint32_t>(tok);
        const auto y        = next_uint<uint32_t>(tok);
        if (!eggId || !playerId || !x || !y) {
            return std::nullopt;
        }
        m.eggId    = *eggId;
        m.playerId = *playerId;
        m.x        = *x;
        m.y        = *y;
        return m;
    }

    // ── Player connected via egg ─────────────────────────────────────────────
    if (cmd == "ebo") {
        MsgEggConnection m{};
        const auto eggId = next_id(tok);
        if (!eggId) {
            return std::nullopt;
        }
        m.eggId = *eggId;
        return m;
    }

    // ── Egg death ────────────────────────────────────────────────────────────
    if (cmd == "edi") {
        MsgEggDeath m{};
        const auto eggId = next_id(tok);
        if (!eggId) {
            return std::nullopt;
        }
        m.eggId = *eggId;
        return m;
    }

    // ── Time unit query response ─────────────────────────────────────────────
    if (cmd == "sgt") {
        MsgTimeUnit m{};
        const auto t = next_uint<uint32_t>(tok);
        if (!t) {
            return std::nullopt;
        }
        m.t = *t;
        return m;
    }

    // ── Time unit changed ────────────────────────────────────────────────────
    if (cmd == "sst") {
        MsgTimeUnitSet m{};
        const auto t = next_uint<uint32_t>(tok);
        if (!t) {
            return std::nullopt;
        }
        m.t = *t;
        return m;
    }

    // ── End of game ──────────────────────────────────────────────────────────
    if (cmd == "seg") {
        MsgEndGame m{};
        const auto team = tok.next();
        if (!team) {
            return std::nullopt;
        }
        m.team = std::string(*team);
        return m;
    }

    // ── Server message ───────────────────────────────────────────────────────
    if (cmd == "smg") {
        MsgServerMessage m{};
        // Free-form text; may contain spaces. Everything after "smg " is the message.
        m.message = std::string(tok.remainder_trimmed());
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
