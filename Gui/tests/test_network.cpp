/**
 * @file tests/test_network.cpp
 * @brief Integration test for NetworkClient — connects to a mock server and
 *        verifies that the correct ServerMessage types appear in the queue.
 * @details Compile (from Gui/):
 *            g++ -std=c++20 -Wall -Wextra -I src \
 *                tests/test_network.cpp src/network/NetworkClient.cpp \
 *                -o build/test_network -pthread -lspdlog  (system spdlog)
 *
 *          Or via CMake after adding test_network target (see CMakeLists addition).
 *
 *          Usage:
 *            1. Terminal A: python3 tests/mock_server.py 14242
 *            2. Terminal B: ./build/test_network
 *
 *          The test exits 0 on success, 1 on any assertion failure or timeout.
 */

#include "network/NetworkClient.hpp"
#include "network/ServerMessage.hpp"
#include "exceptions.hpp"

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>
#include <functional>

// ── Assertion helpers ─────────────────────────────────────────────────────────

static int g_passed = 0;
static int g_failed = 0;

#define EXPECT_TRUE(cond, msg)                                                    \
    do {                                                                          \
        if (!(cond)) {                                                            \
            std::fprintf(stderr, "FAIL  %s  [%s:%d]\n", (msg), __FILE__, __LINE__); \
            ++g_failed;                                                           \
        } else {                                                                  \
            std::printf("PASS  %s\n", (msg));                                    \
            ++g_passed;                                                           \
        }                                                                         \
    } while (0)

// ── Drain helper ─────────────────────────────────────────────────────────────

/**
 * Drain messages from client for up to `max_ms` milliseconds,
 * collecting all received ServerMessage values.
 */
static std::vector<ServerMessage> drain(NetworkClient& client, int max_ms)
{
    std::vector<ServerMessage> msgs;
    auto deadline = std::chrono::steady_clock::now()
                  + std::chrono::milliseconds(max_ms);

    while (std::chrono::steady_clock::now() < deadline) {
        try {
            while (auto m = client.poll()) {
                msgs.push_back(std::move(*m));
            }
        } catch (const NetworkRecvException&) {
            // Server closed — stop draining
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return msgs;
}

/**
 * Return true if at least one element of msgs holds type T matching predicate.
 */
template <typename T>
static bool has(const std::vector<ServerMessage>& msgs,
                std::function<bool(const T&)> pred)
{
    for (const auto& m : msgs) {
        if (std::holds_alternative<T>(m)) {
            if (pred(std::get<T>(m))) return true;
        }
    }
    return false;
}

// ── Test runner ───────────────────────────────────────────────────────────────

int main()
{
    std::printf("=== NetworkClient Integration Tests (port 14242) ===\n\n");

    // ── Test 1: connect() succeeds ────────────────────────────────────────────
    NetworkClient client("127.0.0.1", 14242);
    bool connected = false;
    try {
        client.connect();
        connected = true;
    } catch (const NetworkConnectException& e) {
        std::fprintf(stderr, "FAIL  connect() threw: %s\n", e.what());
        ++g_failed;
    }
    EXPECT_TRUE(connected, "connect() succeeds without throwing");
    if (!connected) {
        std::printf("\n=== Results: %d passed, %d failed ===\n", g_passed, g_failed);
        return 1;
    }

    // ── Drain messages for up to 3 seconds ────────────────────────────────────
    std::vector<ServerMessage> msgs = drain(client, 3000);

    std::printf("\n[test] received %zu messages total\n\n", msgs.size());

    // ── Test 2: at least one message was received ─────────────────────────────
    EXPECT_TRUE(!msgs.empty(), "at least one message received from mock server");

    // ── Test 3: MsgMapSize — msz 10 8 ────────────────────────────────────────
    EXPECT_TRUE(has<MsgMapSize>(msgs, [](const MsgMapSize& m) {
        return m.x == 10 && m.y == 8;
    }), "MsgMapSize: x=10, y=8 received");

    // ── Test 4: MsgTileContent — bct 0 0 1 0 0 0 0 0 0 ───────────────────────
    EXPECT_TRUE(has<MsgTileContent>(msgs, [](const MsgTileContent& m) {
        return m.x == 0 && m.y == 0 && m.resources[0] == 1;
    }), "MsgTileContent: bct 0 0 parsed (food=1)");

    // ── Test 5: MsgTileContent — bct 3 4 with linemate=2, deraumere=1 ─────────
    EXPECT_TRUE(has<MsgTileContent>(msgs, [](const MsgTileContent& m) {
        return m.x == 3 && m.y == 4 && m.resources[1] == 2 && m.resources[2] == 1;
    }), "MsgTileContent: bct 3 4 resources parsed correctly");

    // ── Test 6: MsgTeamName — tna TeamA ──────────────────────────────────────
    EXPECT_TRUE(has<MsgTeamName>(msgs, [](const MsgTeamName& m) {
        return m.name == "TeamA";
    }), "MsgTeamName: TeamA received");

    // ── Test 7: MsgTeamName — tna TeamB ──────────────────────────────────────
    EXPECT_TRUE(has<MsgTeamName>(msgs, [](const MsgTeamName& m) {
        return m.name == "TeamB";
    }), "MsgTeamName: TeamB received");

    // ── Test 8: MsgTimeUnit — sgt 100 ────────────────────────────────────────
    EXPECT_TRUE(has<MsgTimeUnit>(msgs, [](const MsgTimeUnit& m) {
        return m.t == 100;
    }), "MsgTimeUnit: sgt 100 received");

    // ── Test 9: MsgPlayerNew — pnw #1 2 3 1 1 TeamA ──────────────────────────
    EXPECT_TRUE(has<MsgPlayerNew>(msgs, [](const MsgPlayerNew& m) {
        return m.id == 1 && m.x == 2 && m.y == 3
            && m.orientation == 1 && m.level == 1
            && m.team == "TeamA";
    }), "MsgPlayerNew: pnw #1 parsed correctly");

    // ── Test 10: MsgPlayerPosition — ppo #1 5 6 2 ────────────────────────────
    EXPECT_TRUE(has<MsgPlayerPosition>(msgs, [](const MsgPlayerPosition& m) {
        return m.id == 1 && m.x == 5 && m.y == 6 && m.orientation == 2;
    }), "MsgPlayerPosition: ppo #1 parsed correctly");

    // ── Test 11: MsgPlayerLevel — plv #1 3 ────────────────────────────────────
    EXPECT_TRUE(has<MsgPlayerLevel>(msgs, [](const MsgPlayerLevel& m) {
        return m.id == 1 && m.level == 3;
    }), "MsgPlayerLevel: plv #1 parsed correctly");

    // ── Test 12: MsgPlayerInventory — pin #1 5 6 10 0 1 0 0 0 0 ──────────────
    EXPECT_TRUE(has<MsgPlayerInventory>(msgs, [](const MsgPlayerInventory& m) {
        return m.id == 1 && m.x == 5 && m.y == 6
            && m.resources[0] == 10 && m.resources[2] == 1;
    }), "MsgPlayerInventory: pin #1 parsed correctly");

    // ── Test 13: MsgPlayerExpulsion — pex #1 ─────────────────────────────────
    EXPECT_TRUE(has<MsgPlayerExpulsion>(msgs, [](const MsgPlayerExpulsion& m) {
        return m.id == 1;
    }), "MsgPlayerExpulsion: pex #1 parsed correctly");

    // ── Test 14: MsgPlayerBroadcast — pbc #1 hello world ─────────────────────
    EXPECT_TRUE(has<MsgPlayerBroadcast>(msgs, [](const MsgPlayerBroadcast& m) {
        return m.id == 1 && m.message == "hello world";
    }), "MsgPlayerBroadcast: pbc message with spaces parsed correctly");

    // ── Test 15: MsgPlayerDeath — pdi #1 ─────────────────────────────────────
    EXPECT_TRUE(has<MsgPlayerDeath>(msgs, [](const MsgPlayerDeath& m) {
        return m.id == 1;
    }), "MsgPlayerDeath: pdi #1 parsed correctly");

    // ── Test 16: MsgIncantationStart — pic 2 3 2 #1 #2 ───────────────────────
    EXPECT_TRUE(has<MsgIncantationStart>(msgs, [](const MsgIncantationStart& m) {
        return m.x == 2 && m.y == 3 && m.level == 2
            && m.players.size() == 2
            && m.players[0] == 1 && m.players[1] == 2;
    }), "MsgIncantationStart: pic parsed with 2 players");

    // ── Test 17: MsgIncantationEnd — pie 2 3 ok ───────────────────────────────
    EXPECT_TRUE(has<MsgIncantationEnd>(msgs, [](const MsgIncantationEnd& m) {
        return m.x == 2 && m.y == 3 && m.result == true;
    }), "MsgIncantationEnd: pie ok parsed correctly");

    // ── Test 18: MsgEggLaying — pfk #1 ───────────────────────────────────────
    EXPECT_TRUE(has<MsgEggLaying>(msgs, [](const MsgEggLaying& m) {
        return m.playerId == 1;
    }), "MsgEggLaying: pfk #1 parsed correctly");

    // ── Test 19: MsgResourceDrop — pdr #1 2 ──────────────────────────────────
    EXPECT_TRUE(has<MsgResourceDrop>(msgs, [](const MsgResourceDrop& m) {
        return m.playerId == 1 && m.resource == 2;
    }), "MsgResourceDrop: pdr #1 2 parsed correctly");

    // ── Test 20: MsgResourceCollect — pgt #1 3 ───────────────────────────────
    EXPECT_TRUE(has<MsgResourceCollect>(msgs, [](const MsgResourceCollect& m) {
        return m.playerId == 1 && m.resource == 3;
    }), "MsgResourceCollect: pgt #1 3 parsed correctly");

    // ── Test 21: MsgEggLaid — enw #10 #1 2 3 ────────────────────────────────
    EXPECT_TRUE(has<MsgEggLaid>(msgs, [](const MsgEggLaid& m) {
        return m.eggId == 10 && m.playerId == 1 && m.x == 2 && m.y == 3;
    }), "MsgEggLaid: enw parsed correctly");

    // ── Test 22: MsgEggConnection — ebo #10 ──────────────────────────────────
    EXPECT_TRUE(has<MsgEggConnection>(msgs, [](const MsgEggConnection& m) {
        return m.eggId == 10;
    }), "MsgEggConnection: ebo #10 parsed correctly");

    // ── Test 23: MsgEggDeath — edi #10 ───────────────────────────────────────
    EXPECT_TRUE(has<MsgEggDeath>(msgs, [](const MsgEggDeath& m) {
        return m.eggId == 10;
    }), "MsgEggDeath: edi #10 parsed correctly");

    // ── Test 24: MsgTimeUnitSet — sst 50 ─────────────────────────────────────
    EXPECT_TRUE(has<MsgTimeUnitSet>(msgs, [](const MsgTimeUnitSet& m) {
        return m.t == 50;
    }), "MsgTimeUnitSet: sst 50 parsed correctly");

    // ── Test 25: MsgEndGame — seg TeamA ──────────────────────────────────────
    EXPECT_TRUE(has<MsgEndGame>(msgs, [](const MsgEndGame& m) {
        return m.team == "TeamA";
    }), "MsgEndGame: seg TeamA parsed correctly");

    // ── Test 26: MsgServerMessage — smg hello from server ────────────────────
    EXPECT_TRUE(has<MsgServerMessage>(msgs, [](const MsgServerMessage& m) {
        return m.message == "hello from server";
    }), "MsgServerMessage: smg with spaces parsed correctly");

    // ── Test 27: MsgUnknownCommand — suc ─────────────────────────────────────
    EXPECT_TRUE(has<MsgUnknownCommand>(msgs, [](const MsgUnknownCommand&) {
        return true;
    }), "MsgUnknownCommand: suc parsed correctly");

    // ── Test 28: MsgBadParameter — sbp ───────────────────────────────────────
    EXPECT_TRUE(has<MsgBadParameter>(msgs, [](const MsgBadParameter&) {
        return true;
    }), "MsgBadParameter: sbp parsed correctly");

    // ── Test 29: unknown prefix ("xyz") does NOT appear as any typed message ──
    // It should have been silently dropped (no MsgUnknownCommand for it — suc does that).
    // We can only verify this indirectly: count MsgUnknownCommand entries == 1 (from suc).
    {
        int suc_count = 0;
        for (const auto& m : msgs) {
            if (std::holds_alternative<MsgUnknownCommand>(m)) ++suc_count;
        }
        EXPECT_TRUE(suc_count == 1,
                    "unknown prefix 'xyz' is silently dropped (only 1 MsgUnknownCommand from suc)");
    }

    // ── Test 30: hasError() is false after clean server close ─────────────────
    // The mock server closes cleanly after sending all messages.
    // Give the recv thread a moment to detect the close.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    // After a clean close, _hadError is set. poll() would throw NetworkRecvException.
    // hasError() should return true (the server closed, which is an "error" from our POV).
    // This verifies the detection path works.
    bool err = client.hasError();
    EXPECT_TRUE(err, "hasError() returns true after server closes connection");

    std::printf("\n=== Results: %d passed, %d failed ===\n", g_passed, g_failed);
    return (g_failed == 0) ? 0 : 1;
}
