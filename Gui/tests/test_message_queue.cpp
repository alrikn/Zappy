/**
 * @file tests/test_message_queue.cpp
 * @brief Unit tests for MessageQueue<T> — no Vulkan, no network required.
 * @details Tests are structured as plain assertions that call std::terminate
 *          with a descriptive message on failure.  Each section is independent.
 *
 *          Compile (from Gui/):
 *            g++ -std=c++20 -Wall -Wextra -I src \
 *                tests/test_message_queue.cpp -o build/test_message_queue -pthread
 *          Run:
 *            ./build/test_message_queue
 */

#include "network/MessageQueue.hpp"
#include "network/ServerMessage.hpp"

#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>
#include <atomic>

// ── Test helpers ──────────────────────────────────────────────────────────────

static int g_passed = 0;
static int g_failed = 0;

#define EXPECT_TRUE(cond, msg)                                              \
    do {                                                                    \
        if (!(cond)) {                                                      \
            std::fprintf(stderr, "FAIL  %s  [%s:%d]\n", (msg), __FILE__, __LINE__); \
            ++g_failed;                                                     \
        } else {                                                            \
            std::printf("PASS  %s\n", (msg));                              \
            ++g_passed;                                                     \
        }                                                                   \
    } while (0)

#define EXPECT_EQ(a, b, msg) EXPECT_TRUE((a) == (b), (msg))

// ── Test 1: empty queue returns nullopt ───────────────────────────────────────

static void test_empty_queue()
{
    MessageQueue<int> q;
    EXPECT_TRUE(!q.tryPop().has_value(), "empty queue: tryPop returns nullopt");
    EXPECT_EQ(q.size(), 0u, "empty queue: size == 0");
}

// ── Test 2: push / tryPop round-trip ─────────────────────────────────────────

static void test_push_pop_int()
{
    MessageQueue<int> q;
    q.push(42);
    q.push(7);

    EXPECT_EQ(q.size(), 2u, "push x2: size == 2");

    auto v1 = q.tryPop();
    EXPECT_TRUE(v1.has_value(), "tryPop: has value after push");
    EXPECT_EQ(*v1, 42, "tryPop: FIFO order — first value == 42");

    auto v2 = q.tryPop();
    EXPECT_TRUE(v2.has_value(), "tryPop: second value present");
    EXPECT_EQ(*v2, 7, "tryPop: FIFO order — second value == 7");

    auto v3 = q.tryPop();
    EXPECT_TRUE(!v3.has_value(), "tryPop: empty after draining returns nullopt");
    EXPECT_EQ(q.size(), 0u, "size == 0 after draining");
}

// ── Test 3: push / tryPop with ServerMessage variant ─────────────────────────

static void test_with_server_message()
{
    MessageQueue<ServerMessage> q;

    MsgMapSize ms;
    ms.x = 10;
    ms.y = 8;
    q.push(ms);

    MsgTeamName tn;
    tn.name = "TeamA";
    q.push(tn);

    EXPECT_EQ(q.size(), 2u, "ServerMessage queue: size == 2");

    auto m1 = q.tryPop();
    EXPECT_TRUE(m1.has_value(), "ServerMessage: first pop has value");
    EXPECT_TRUE(std::holds_alternative<MsgMapSize>(*m1),
                "ServerMessage: first item is MsgMapSize");
    EXPECT_EQ(std::get<MsgMapSize>(*m1).x, 10u,
              "ServerMessage: MsgMapSize.x == 10");
    EXPECT_EQ(std::get<MsgMapSize>(*m1).y, 8u,
              "ServerMessage: MsgMapSize.y == 8");

    auto m2 = q.tryPop();
    EXPECT_TRUE(m2.has_value(), "ServerMessage: second pop has value");
    EXPECT_TRUE(std::holds_alternative<MsgTeamName>(*m2),
                "ServerMessage: second item is MsgTeamName");
    EXPECT_EQ(std::get<MsgTeamName>(*m2).name, std::string("TeamA"),
              "ServerMessage: MsgTeamName.name == TeamA");
}

// ── Test 4: large number of pushes ───────────────────────────────────────────

static void test_many_pushes()
{
    MessageQueue<int> q;
    const int N = 10000;
    for (int i = 0; i < N; ++i) {
        q.push(i);
    }
    EXPECT_EQ(q.size(), static_cast<std::size_t>(N), "many pushes: size == N");

    for (int i = 0; i < N; ++i) {
        auto v = q.tryPop();
        if (!v.has_value() || *v != i) {
            EXPECT_TRUE(false, "many pushes: FIFO order broken");
            return;
        }
    }
    EXPECT_TRUE(true, "many pushes: all 10000 items in FIFO order");
    EXPECT_EQ(q.size(), 0u, "many pushes: empty after draining");
}

// ── Test 5: concurrent push / tryPop ─────────────────────────────────────────

static void test_concurrent()
{
    // Producer pushes 1000 ints; consumer pops them.
    // We verify no item is lost and no crash occurs.
    const int N = 1000;
    MessageQueue<int> q;
    std::atomic<int> popped_count{0};

    std::thread producer([&] {
        for (int i = 0; i < N; ++i) {
            q.push(i);
        }
    });

    std::thread consumer([&] {
        int count = 0;
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (count < N && std::chrono::steady_clock::now() < deadline) {
            if (auto v = q.tryPop()) {
                ++count;
            }
        }
        popped_count.store(count);
    });

    producer.join();
    consumer.join();

    EXPECT_EQ(popped_count.load(), N, "concurrent: all 1000 items consumed");
    EXPECT_EQ(q.size(), 0u, "concurrent: queue empty after consumer finishes");
}

// ── Test 6: move semantics — string content is preserved ──────────────────────

static void test_move_semantics()
{
    MessageQueue<std::string> q;
    std::string s = "hello_world_long_enough_to_force_heap_allocation";
    q.push(s);  // push by value (copy)

    std::string s2 = "another_string";
    q.push(std::move(s2));  // push by move

    auto v1 = q.tryPop();
    EXPECT_TRUE(v1.has_value() && *v1 == "hello_world_long_enough_to_force_heap_allocation",
                "move semantics: copied string preserved");

    auto v2 = q.tryPop();
    EXPECT_TRUE(v2.has_value() && *v2 == "another_string",
                "move semantics: moved string preserved");
}

// ── Test 7: size() is consistent under lock ───────────────────────────────────

static void test_size_consistency()
{
    MessageQueue<int> q;
    // Interleave pushes and size checks
    q.push(1);
    EXPECT_EQ(q.size(), 1u, "size consistency: after 1 push");
    q.push(2);
    q.push(3);
    EXPECT_EQ(q.size(), 3u, "size consistency: after 3 pushes");
    (void)q.tryPop();
    EXPECT_EQ(q.size(), 2u, "size consistency: after 1 pop");
    (void)q.tryPop();
    (void)q.tryPop();
    EXPECT_EQ(q.size(), 0u, "size consistency: after draining all");
}

// ── Test 8: edge — single push then pop returns nullopt ─────────────────────

static void test_single_element_cycle()
{
    MessageQueue<int> q;
    q.push(99);
    auto v = q.tryPop();
    EXPECT_TRUE(v.has_value() && *v == 99, "single element cycle: pop returns 99");
    auto empty = q.tryPop();
    EXPECT_TRUE(!empty.has_value(), "single element cycle: second pop is nullopt");
}

// ── Test 9: MsgTileContent resources array integrity ─────────────────────────

static void test_tile_content_resources()
{
    MessageQueue<ServerMessage> q;
    MsgTileContent tc{};
    tc.x = 3;
    tc.y = 7;
    tc.resources = {1, 2, 3, 4, 5, 6, 7};
    q.push(tc);

    auto v = q.tryPop();
    EXPECT_TRUE(v.has_value(), "MsgTileContent: pop has value");
    EXPECT_TRUE(std::holds_alternative<MsgTileContent>(*v),
                "MsgTileContent: correct variant type");

    const auto& got = std::get<MsgTileContent>(*v);
    EXPECT_EQ(got.x, 3u, "MsgTileContent: x == 3");
    EXPECT_EQ(got.y, 7u, "MsgTileContent: y == 7");
    bool resources_ok = (got.resources[0] == 1 && got.resources[6] == 7);
    EXPECT_TRUE(resources_ok, "MsgTileContent: resources array preserved");
}

// ── Test 10: MsgIncantationStart players vector ───────────────────────────────

static void test_incantation_players_vector()
{
    MessageQueue<ServerMessage> q;
    MsgIncantationStart inc{};
    inc.x = 5;
    inc.y = 5;
    inc.level = 2;
    inc.players = {1, 2, 3};
    q.push(inc);

    auto v = q.tryPop();
    EXPECT_TRUE(v.has_value(), "MsgIncantationStart: pop has value");
    EXPECT_TRUE(std::holds_alternative<MsgIncantationStart>(*v),
                "MsgIncantationStart: correct variant type");

    const auto& got = std::get<MsgIncantationStart>(*v);
    EXPECT_EQ(got.players.size(), 3u, "MsgIncantationStart: 3 players");
    EXPECT_EQ(got.players[0], 1u, "MsgIncantationStart: player[0] == 1");
    EXPECT_EQ(got.players[2], 3u, "MsgIncantationStart: player[2] == 3");
}

// ── main ──────────────────────────────────────────────────────────────────────

int main()
{
    std::printf("=== MessageQueue Unit Tests ===\n\n");

    test_empty_queue();
    test_push_pop_int();
    test_with_server_message();
    test_many_pushes();
    test_concurrent();
    test_move_semantics();
    test_size_consistency();
    test_single_element_cycle();
    test_tile_content_resources();
    test_incantation_players_vector();

    std::printf("\n=== Results: %d passed, %d failed ===\n", g_passed, g_failed);
    return (g_failed == 0) ? 0 : 1;
}
