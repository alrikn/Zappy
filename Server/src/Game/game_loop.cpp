/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** main_loop
*/

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
** we should be able to find at least on of each resource and food on the floor
** resources should be spread evenly accross the map
** the ressource quantity can be found with the following formula: map_width * map_height * density
*/
void Server::populate_map_resources()
{
    static const double densities[] = {
        FOOD_DENSITY, LINEMATE_DENSITY, DERAUMERE_DENSITY, SIBUR_DENSITY,
        MENDIANE_DENSITY, PHIRAS_DENSITY, THYSTAME_DENSITY};

    //pdf: the map must hold at least width*height*density of each resource
    // so we count whats already on the floor and only drop the missing amount,
    // spread over random tiles. (the old version blindly readded every cycle and
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

void Server::advance_game()
{
    auto now = std::chrono::steady_clock::now();

    // resource respawn on 20tu deadline
    while (now >= _next_respawn_at) {
        populate_map_resources();
        _next_respawn_at += std::chrono::milliseconds(20 * time_unit);
    }

    // collect players to kill after the loop to avoid iterator invalidation
    std::vector<std::shared_ptr<Player>> to_kill;

    for (auto &[fd, client] : _clients) {
        (void)fd;
        if (client->get_type() != PLAYER)
            continue;
        std::shared_ptr<Player> player = std::dynamic_pointer_cast<Player>(client);
        if (!player)
            continue;

        // initialise food timer on first sight
        if (player->next_food_at == std::chrono::steady_clock::time_point{})
            player->next_food_at = now + std::chrono::milliseconds(126 * time_unit);

        // food drain: each tick of 126tu costs 1 food
        while (now >= player->next_food_at) {
            if (player->inventory.resources[static_cast<size_t>(Resource::Food)] > 0) {
                player->inventory.resources[static_cast<size_t>(Resource::Food)]--;
                player->next_food_at += std::chrono::milliseconds(126 * time_unit);
            } else {
                to_kill.push_back(player);
                break;
            }
        }

        // finish incantation phase 2 when its 300tu deadline elapses
        if (player->busy && now >= player->action_done_at) {
            player->busy = false;
            player->execute_command(player->running_cmd.first,
                player->running_cmd.second, *this);
        }
        // exe next queued cmd immediately
        if (!player->busy && !player->in_incantation && !player->cmd_queue.empty()) {
            auto [verb, args] = player->cmd_queue.front();
            player->cmd_queue.pop_front();
            if (verb == INCANTATION)
                player->incantation_start(*this);
            else
                player->execute_command(verb, args, *this);
        }
    }

    for (auto &dead : to_kill)
        kill_player(dead);
}

void Server::run()
{
    auto next_tick = std::chrono::steady_clock::now();

    while (!g_shutdown_requested && running) {
        auto now = std::chrono::steady_clock::now();
        if (now >= next_tick) {
            game_tick();
            next_tick += std::chrono::milliseconds(time_unit);
        }
        now = std::chrono::steady_clock::now();
        int timeout = std::chrono::duration_cast<std::chrono::milliseconds>(next_tick - now).count();
        if (timeout < 0)
            timeout = 0;
        poll_clients(timeout); //blocks until socket activity or the next tick
        advance_game();        //run any commands that came in
    }
}