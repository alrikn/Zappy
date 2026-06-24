/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** Server
*/

#include "Server.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>

Server::Server(int port_number,
    int map_width,
    int map_height,
    std::vector<std::string> team_names,
    int num_client_per_team,
    long long trantorian_time_unit)
    : _network(port_number),
      _clients(_network._clients),
      send_message_queue(_network.send_message_queue)
{
    _freq = trantorian_time_unit;
    time_unit = static_cast<long long>(1000.0 / trantorian_time_unit);
    if (time_unit < 1)
        throw std::runtime_error("Invalid time unit: " + std::to_string(time_unit));
    std::cout << "time unit: " << time_unit << std::endl;

    tick = 0;
    _map = std::vector<std::vector<Tiles>>(map_height, std::vector<Tiles>(map_width, Tiles()));

    for (const std::string &name : team_names)
        teams.push_back(std::make_shared<Team>(name, num_client_per_team));

    for (auto &team : teams) {
        for (int i = 0; i < num_client_per_team; i++) {
            int x = rand() % map_width;
            int y = rand() % map_height;
            team->eggs.push_back(
                std::make_shared<Egg>(team->name, std::vector<int>{x, y}));
        }
    }

    //wire network to game callbacks
    _network._on_team_name = [this](int fd, const std::string &team_name) {
        finalize_client(fd, team_name);
    };
    _network._on_command = [this](int fd, const std::string &cmd) {
        auto it = _clients.find(fd);
        if (it != _clients.end())
            it->second->parse_command(cmd, *this);
    };
    _network._on_disconnect = [this](int fd) {
        remove_client(fd);
    };

    populate_map_resources();
}
