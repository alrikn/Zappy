/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** main_loop
*/

#include "Gui.hpp"
#include "Parse.hpp"
#include "Server.hpp"
#include "Struct.hpp"

#include <csignal>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <memory>
#include <chrono>

extern volatile sig_atomic_t g_shutdown_requested;

/*
** we should be able to find at least on of each rsrc and food on the floor
** rsrc should be spread evenly accross the map
** the rsrc quantity can be found with: map_width * map_height * density
*/
void Server::populate_map_resources()
{
    static const double densities[] = {
        FOOD_DENSITY, LINEMATE_DENSITY, DERAUMERE_DENSITY, SIBUR_DENSITY,
        MENDIANE_DENSITY, PHIRAS_DENSITY, THYSTAME_DENSITY};

    //pdf: the map must hold at least width*height*density of each rsrc
    // so we count whats already on the floor and only drop the missing amount
    // spread over random tiles
    // drove its running total negative -> rand() % 0 -> SIGFPE
    for (size_t r = 0; r < static_cast<size_t>(Resource::Count); r++) {
        int target = static_cast<int>(getMapWidth() * getMapHeight() * densities[r]);
        int current = 0;
        for (const auto &row : _map)
            for (const auto &tile : row)
                current += tile.inventory.resources[r];
        for (int n = current; n < target; n++) {
            int x = rand() % getMapWidth();
            int y = rand() % getMapHeight();
            _map[y][x].inventory.resources[r]++;
        }
    }
}

void Server::game_tick()
{
    tick++;
}

// respawn the floor rsrc every RESPAWN_TICKS ticks
void Server::respawn_resources()
{
    if (tick - _last_respawn_tick < RESPAWN_TICKS)
        return;

    // snapshot inventories before adding resources
    int W = getMapWidth(), H = getMapHeight();
    std::vector<std::vector<Inventory>> before(H, std::vector<Inventory>(W));
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            before[y][x] = _map[y][x].inventory;

    populate_map_resources();
    std::cout << "tick: " << tick << std::endl;
    _last_respawn_tick = tick;

    // notify GUI only for tiles that actually changed
    for (int x = 0; x < W; x++) {
        for (int y = 0; y < H; y++) {
            if (before[y][x].resources != _map[y][x].inventory.resources) {
                _gui_subject.Notify([this, x, y](Client* c) {
                    static_cast<Gui*>(c)->bct(*this, {std::to_string(x), std::to_string(y)});
                });
            }
        }
    }
}

// one food unit lasts FOOD_DRAIN_TICKS ticks, a player with no food left to drain
// is queued in to_kill (we kill after the player loop to not invalidate iterators)
void Server::drain_food(std::shared_ptr<Player> player,
    std::vector<std::shared_ptr<Player>> &to_kill)
{
    if (player->next_food_at == 0) //arm the timer the first time we see the player
        player->next_food_at = tick + FOOD_DRAIN_TICKS;
    while (tick >= player->next_food_at) {
        if (player->inventory.resources[static_cast<size_t>(Resource::Food)] > 0) {
            player->inventory.resources[static_cast<size_t>(Resource::Food)]--;
            player->next_food_at += FOOD_DRAIN_TICKS;
            _gui_subject.Notify([player, this](Client* c) {
                static_cast<Gui*>(c)->pin(*this, {std::to_string(player->getId())});
            });
        } else {
            to_kill.push_back(player);
            break;
        }
    }
}

// we check buisiness here.
void Server::step_player_action(std::shared_ptr<Player> player)
{
    //first we undo buisness if the player is not done
    if (player->busy && tick >= player->action_done_at) {
        player->busy = false;
        if (player->in_incantation) {
            player->incantation_end(*this);
        }
    }

    //then we check if the player is free to execute a new command
    if (!player->busy && !player->cmd_queue.empty()) {
        player->running_cmd = player->cmd_queue.front();
        player->cmd_queue.pop_front();
        player->busy = true;
        player->action_done_at = tick + ClientCommandDelayMap.at(player->running_cmd.first);
        player->execute_command(player->running_cmd.first, player->running_cmd.second, *this);
    }
}

void Server::advance_game()
{
    respawn_resources();
    //we update player actions
    send_message_queue.send_messages(tick);

    // collect players to kill after the loop to avoid iterator invalidation
    std::vector<std::shared_ptr<Player>> to_kill;
    for (auto &[fd, client] : _clients) {
        (void)fd;
        if (client->get_type() != PLAYER)
            continue;
        std::shared_ptr<Player> player = std::dynamic_pointer_cast<Player>(client);
        if (!player)
            continue;
        drain_food(player, to_kill);
        step_player_action(player);
    }

    for (auto &dead : to_kill)
        kill_player(dead);
}

void Server::run()
{
    //the only wall clock in the engine: it paces the tick counter at one tick per
    //time_unit ms, everything downstream (food, incantation, respawn) counts ticks
    std::chrono::time_point<std::chrono::steady_clock> next_tick = std::chrono::steady_clock::now();
    std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();
    int timeout = 0;

    while (!g_shutdown_requested && running) {
        try {
            now = std::chrono::steady_clock::now();
            timeout = std::chrono::duration_cast<std::chrono::milliseconds>(next_tick - now).count();
        if (timeout < 0)
            timeout = 0;
        } catch (const std::exception &e) {
            std::cerr << "Error calculating timeout: " << e.what() << std::endl;
            timeout = 0;
        }
        try {
            poll_clients(timeout); //blocks until socket activity or the next tick is due
        } catch (const std::exception &e) {
            std::cerr << "Error during poll_clients: " << e.what() << std::endl;
            continue;
        }

        try {
            now = std::chrono::steady_clock::now();
            while (now >= next_tick) { //advance one game tick per elapsed time_unit
                game_tick();
                advance_game();
                next_tick += std::chrono::milliseconds(time_unit);
            }
        } catch (const std::exception &e) {
            std::cerr << "Error during game tick: " << e.what() << std::endl;
            continue;
        }
    }
}