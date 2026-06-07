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
    resources_t total_resources = {0};
    total_resources.food = _map.size() * _map[0].size() * FOOD_DENSITY;
    total_resources.linemate = _map.size() * _map[0].size() * LINEMATE_DENSITY;
    total_resources.deraumere = _map.size() * _map[0].size() * DERAUMERE_DENSITY;
    total_resources.sibur = _map.size() * _map[0].size() * SIBUR_DENSITY;
    total_resources.mendiane = _map.size() * _map[0].size() * MENDIANE_DENSITY;
    total_resources.phiras = _map.size() * _map[0].size() * PHIRAS_DENSITY;
    total_resources.thystame = _map.size() * _map[0].size() * THYSTAME_DENSITY;

    //terrible way to spread them evenly but for now it'll do
    for (size_t i = 0; i < _map.size(); i++) {
        for (size_t j = 0; j < _map[i].size(); j++) {
            _map[i][j].resources.food += rand() % (total_resources.food + 1);
            total_resources.food -= _map[i][j].resources.food;

            _map[i][j].resources.linemate += rand() % (total_resources.linemate + 1);
            total_resources.linemate -= _map[i][j].resources.linemate;

            _map[i][j].resources.deraumere += rand() % (total_resources.deraumere + 1);
            total_resources.deraumere -= _map[i][j].resources.deraumere;

            _map[i][j].resources.sibur += rand() % (total_resources.sibur + 1);
            total_resources.sibur -= _map[i][j].resources.sibur;

            _map[i][j].resources.mendiane += rand() % (total_resources.mendiane + 1);
            total_resources.mendiane -= _map[i][j].resources.mendiane;

            _map[i][j].resources.phiras += rand() % (total_resources.phiras + 1);
            total_resources.phiras -= _map[i][j].resources.phiras;

            _map[i][j].resources.thystame += rand() % (total_resources.thystame + 1);
            total_resources.thystame -= _map[i][j].resources.thystame;
        }
    }
}

void Server::game_tick()
{
    if (tick % 20 == 0 || tick == 0) { //every 20 ticks we populate the map with resources
        populate_map_resources();
    }
    tick++;
}

void Server::run()
{
    auto next_tick = std::chrono::steady_clock::now();

    while (!g_shutdown_requested && running) {
        auto now = std::chrono::steady_clock::now();
        if (now >= next_tick) {
            game_tick();
            std::cout << "tick: " << tick << std::endl;
            next_tick += std::chrono::milliseconds(time_unit);
        }
        now = std::chrono::steady_clock::now();
        int timeout = std::chrono::duration_cast<std::chrono::milliseconds>(next_tick - now).count();
        if (timeout < 0)
            timeout = 0;
        poll_clients(timeout);
    }
}