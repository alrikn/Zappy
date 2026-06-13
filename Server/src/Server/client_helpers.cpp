/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** client_helpers
*/

#include "Server.hpp"
#include "Client.hpp"
#include "Player.hpp"
#include "Tiles.hpp"
#include "Struct.hpp"




void Server::move_player(Player &player, int new_x, int new_y)
{
    _map[player.position[1]][player.position[0]].remove_specific_client(player.getId());
    player.set_position(new_x, new_y);
    // push the real shared_ptr (look it up by fd), not a copy
    auto it = _clients.find(player.control_fd);
    if (it != _clients.end())
        _map[new_y][new_x].players.push_back(
            std::dynamic_pointer_cast<Player>(it->second));
    // orientation is 1 to 4 in the protocol
    notify_gui("ppo " + std::to_string(player.getId())
        + " " + std::to_string(new_x)
        + " " + std::to_string(new_y)
        + " " + std::to_string(static_cast<int>(player.orientation) + 1) + "\n");
}

Resource parse_resource(const std::string& name)
{
    if (name == "food") return Resource::Food;
    if (name == "linemate") return Resource::Linemate;
    if (name == "deraumere") return Resource::Deraumere;
    if (name == "sibur") return Resource::Sibur;
    if (name == "mendiane") return Resource::Mendiane;
    if (name == "phiras") return Resource::Phiras;
    if (name == "thystame") return Resource::Thystame;

    throw std::runtime_error("Unknown resource");
}

