/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** Incantation
*/

#include "Player.hpp"
#include "Server.hpp"
#include <string>
#include <sys/socket.h>
#include "Server.hpp"

/*
ELEVATION REQUIREMENTS

1->2 : 1 player, 1 linemate
2->3 : 2 players, 1 linemate, 1 deraumere, 1 sibur
3->4 : 2 players, 2 linemate, 0 deraumere, 1 sibur, 0 mendiane, 2 phiras
4->5 : 4 players, 1 linemate, 1 deraumere, 2 sibur, 0 mendiane, 1 phiras
5->6 : 4 players, 1 linemate, 2 deraumere, 1 sibur, 3 mendiane, 0 phiras
6->7 : 6 players, 1 linemate, 2 deraumere, 3 sibur, 0 mendiane, 1 phiras
7->8 : 6 players, 2 linemate, 2 deraumere, 2 sibur, 2 mendiane, 2 phiras, 1 thystame
*/

bool check_requirements(int current_level, int players_on_tile, Inventory tile_inventory)
{
    switch (current_level) {
        case 1:
            return players_on_tile >= 1 && tile_inventory.resources[idx(Resource::Linemate)] >= 1;
        case 2:
            return players_on_tile >= 2 && tile_inventory.resources[idx(Resource::Linemate)] >= 1 && tile_inventory.resources[idx(Resource::Deraumere)] >= 1 && tile_inventory.resources[idx(Resource::Sibur)] >= 1;
        case 3:
            return players_on_tile >= 2 && tile_inventory.resources[idx(Resource::Linemate)] >= 2 && tile_inventory.resources[idx(Resource::Deraumere)] >= 0 && tile_inventory.resources[idx(Resource::Sibur)] >= 1 && tile_inventory.resources[idx(Resource::Mendiane)] >= 0 && tile_inventory.resources[idx(Resource::Phiras)] >= 2;
        case 4:
            return players_on_tile >= 4 && tile_inventory.resources[idx(Resource::Linemate)] >= 1 && tile_inventory.resources[idx(Resource::Deraumere)] >= 1 && tile_inventory.resources[idx(Resource::Sibur)] >= 2 && tile_inventory.resources[idx(Resource::Mendiane)] >= 0 && tile_inventory.resources[idx(Resource::Phiras)] >= 1;
        case 5:
            return players_on_tile >= 4 && tile_inventory.resources[idx(Resource::Linemate)] >= 1 && tile_inventory.resources[idx(Resource::Deraumere)] >= 2 && tile_inventory.resources[idx(Resource::Sibur)] >= 1 && tile_inventory.resources[idx(Resource::Mendiane)] >= 3 && tile_inventory.resources[idx(Resource::Phiras)] >= 0;
        case 6:
            return players_on_tile >= 6 && tile_inventory.resources[idx(Resource::Linemate)] >= 1 && tile_inventory.resources[idx(Resource::Deraumere)] >= 2 && tile_inventory.resources[idx(Resource::Sibur)] >= 3 && tile_inventory.resources[idx(Resource::Mendiane)] >= 0 && tile_inventory.resources[idx(Resource::Phiras)] >= 1;
        case 7:
            return players_on_tile >= 6 && tile_inventory.resources[idx(Resource::Linemate)] >= 2 && tile_inventory.resources[idx(Resource::Deraumere)] >= 2 && tile_inventory.resources[idx(Resource::Sibur)] >= 2 && tile_inventory.resources[idx(Resource::Mendiane)] >= 2 && tile_inventory.resources[idx(Resource::Phiras)] >= 2 && tile_inventory.resources[idx(Resource::Thystame)] >= 1;
        default:
            return false;
    }
}

void Player::incantation(Server &server)
{
    //we check first if player meets requirements.
    int players_on_tile = server._map[position[1]][position[0]].players.size();
    Inventory tile_inventory = server._map[position[1]][position[0]].inventory;

    if (!check_requirements(level, players_on_tile, tile_inventory)) {
        send_message("ko\n");
        return;
    }
    //tell the gui that incant is underway
    auto self = std::dynamic_pointer_cast<Player>(server._clients[control_fd]);
    server._gui_subject.Notify([self, &server](Client* c) {
        static_cast<Gui*>(c)->pic(self->level, server._map[self->position[1]][self->position[0]].players);
    });

    //if they do, we level them up and remove the resources from the tile
    level++;
    switch (level) {
        case 2:
            server._map[position[1]][position[0]].inventory.resources[idx(Resource::Linemate)] -= 1;
            break;
        case 3:
            server._map[position[1]][position[0]].inventory.resources[idx(Resource::Linemate)] -= 2;
            server._map[position[1]][position[0]].inventory.resources[idx(Resource::Deraumere)] -= 1;
            server._map[position[1]][position[0]].inventory.resources[idx(Resource::Sibur)] -= 1;
            break;
        case 4:
            server._map[position[1]][position[0]].inventory.resources[idx(Resource::Linemate)] -= 1;
            server._map[position[1]][position[0]].inventory.resources[idx(Resource::Deraumere)] -= 1;
            server._map[position[1]][position[0]].inventory.resources[idx(Resource::Sibur)] -= 2;
            server._map[position[1]][position[0]].inventory.resources[idx(Resource::Phiras)] -= 1;
            break;
        case 5:
            server._map[position[1]][position[0]].inventory.resources[idx(Resource::Linemate)] -= 1;
            server._map[position[1]][position[0]].inventory.resources[idx(Resource::Deraumere)] -= 2;
            server._map[position[1]][position[0]].inventory.resources[idx(Resource::Sibur)] -= 1;
            server._map[position[1]][position[0]].inventory.resources[idx(Resource::Mendiane)] -= 3;
            break;
        case 6:
            server._map[position[1]][position[0]].inventory.resources[idx(Resource::Linemate)] -= 1;
            server._map[position[1]][position[0]].inventory.resources[idx(Resource::Deraumere)] -= 2;
            server._map[position[1]][position[0]].inventory.resources[idx(Resource::Sibur)] -= 3;
            server._map[position[1]][position[0]].inventory.resources[idx(Resource::Phiras)] -= 1;
            break;
        case 7:
            server._map[position[1]][position[0]].inventory.resources[idx(Resource::Linemate)] -= 2;
            server._map[position[1]][position[0]].inventory.resources[idx(Resource::Deraumere)] -= 2;
            server._map[position[1]][position[0]].inventory.resources[idx(Resource::Sibur)] -= 2;
            server._map[position[1]][position[0]].inventory.resources[idx(Resource::Mendiane)] -= 2;
            server._map[position[1]][position[0]].inventory.resources[idx(Resource::Phiras)] -= 2;
            server._map[position[1]][position[0]].inventory.resources[idx(Resource::Thystame)] -= 1;
            break;
        default:
            break;
    }
    send_message("Elevation underway\n");
    //TODO: we need to figure out a way to wait 300 time units here
    send_message("Current level:" + std::to_string(level) + "\n");
    //notify the gui that the incantation has finished
    server._gui_subject.Notify([self](Client* c) {
        static_cast<Gui*>(c)->pie(self->position[0], self->position[1], true);
    });

}