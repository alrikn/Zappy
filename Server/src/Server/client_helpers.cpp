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
    auto it = _clients.find(player.get_fd());
    if (it != _clients.end())
        _map[new_y][new_x].players.push_back(
            std::dynamic_pointer_cast<Player>(it->second));
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

std::string Server::read_from_client(int client_fd)
{
    char buffer[4096];
    ssize_t n = read(client_fd, buffer, sizeof(buffer) - 1);
    if (n <= 0) {
        remove_client(client_fd);
        return "";
    }
    buffer[n] = '\0';
    //now we remove any \r in the buffer, because the gui sends \r\n and we only want \n
    std::string new_buffer(buffer);
    std::string::size_type pos = 0;
    while ((pos = new_buffer.find('\r', pos)) != std::string::npos) {
        new_buffer.erase(pos, 1);
    }
    return new_buffer;
}

