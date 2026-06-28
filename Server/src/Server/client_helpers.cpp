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




bool Server::move_player(Player &player, int new_x, int new_y)
{
    _map[player.position[1]][player.position[0]].remove_specific_client(player.getId());
    player.set_position(new_x, new_y);
    // push the real shared_ptr (look it up by fd), not a copy
    auto it = _clients.find(player.get_fd());
    auto self = std::dynamic_pointer_cast<Player>(it->second);
    if (it != _clients.end() && self)  // sanity check
        _map[new_y][new_x].players.push_back(
            std::dynamic_pointer_cast<Player>(it->second));
    else
        return false;  // player not found in _clients, should not happen
    //notify gui of the move
    if (self) {
        _gui_subject.Notify([self](Client* c) {
            static_cast<Gui*>(c)->ppo(self);
        });
    }
    return true;
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


