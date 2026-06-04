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




void Server::move_player(Player &player, int new_x, int new_y)
{
    //remove player from old tile
    _map[player.position[1]][player.position[0]].remove_specific_client(player.getId());
    //update player position
    player.set_position(new_x, new_y);
    //add player to new tile
    _map[new_y][new_x].clients.push_back(std::make_shared<Player>(player));
}

