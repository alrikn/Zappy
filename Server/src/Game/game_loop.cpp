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
    Inventory total_resources;
    total_resources.resources[idx(Resource::Food)] = static_cast<int>(getMapHeight() * getMapWidth() * FOOD_DENSITY);
    total_resources.resources[idx(Resource::Linemate)] = static_cast<int>(getMapHeight() * getMapWidth() * LINEMATE_DENSITY);
    total_resources.resources[idx(Resource::Deraumere)] = static_cast<int>(getMapHeight() * getMapWidth() * DERAUMERE_DENSITY);
    total_resources.resources[idx(Resource::Sibur)] = static_cast<int>(getMapHeight() * getMapWidth() * SIBUR_DENSITY);
    total_resources.resources[idx(Resource::Mendiane)] = static_cast<int>(getMapHeight() * getMapWidth() * MENDIANE_DENSITY);
    total_resources.resources[idx(Resource::Phiras)] = static_cast<int>(getMapHeight() * getMapWidth() * PHIRAS_DENSITY);
    total_resources.resources[idx(Resource::Thystame)] = static_cast<int>(getMapHeight() * getMapWidth() * THYSTAME_DENSITY);

    //terrible way to spread them evenly but for now it'll do
    for (size_t i = 0; i < _map.size(); i++) {
        for (size_t j = 0; j < _map[i].size(); j++) {
            _map[i][j].inventory.resources[idx(Resource::Food)] += rand() % (total_resources.resources[idx(Resource::Food)] + 1);
            total_resources.resources[idx(Resource::Food)] -= _map[i][j].inventory.resources[idx(Resource::Food)];

            _map[i][j].inventory.resources[idx(Resource::Linemate)] += rand() % (total_resources.resources[idx(Resource::Linemate)] + 1);
            total_resources.resources[idx(Resource::Linemate)] -= _map[i][j].inventory.resources[idx(Resource::Linemate)];

            _map[i][j].inventory.resources[idx(Resource::Deraumere)] += rand() % (total_resources.resources[idx(Resource::Deraumere)] + 1);
            total_resources.resources[idx(Resource::Deraumere)] -= _map[i][j].inventory.resources[idx(Resource::Deraumere)];

            _map[i][j].inventory.resources[idx(Resource::Sibur)] += rand() % (total_resources.resources[idx(Resource::Sibur)] + 1);
            total_resources.resources[idx(Resource::Sibur)] -= _map[i][j].inventory.resources[idx(Resource::Sibur)];

            _map[i][j].inventory.resources[idx(Resource::Mendiane)] += rand() % (total_resources.resources[idx(Resource::Mendiane)] + 1);
            total_resources.resources[idx(Resource::Mendiane)] -= _map[i][j].inventory.resources[idx(Resource::Mendiane)];

            _map[i][j].inventory.resources[idx(Resource::Phiras)] += rand() % (total_resources.resources[idx(Resource::Phiras)] + 1);
            total_resources.resources[idx(Resource::Phiras)] -= _map[i][j].inventory.resources[idx(Resource::Phiras)];

            _map[i][j].inventory.resources[idx(Resource::Thystame)] += rand() % (total_resources.resources[idx(Resource::Thystame)] + 1);
            total_resources.resources[idx(Resource::Thystame)] -= _map[i][j].inventory.resources[idx(Resource::Thystame)];
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