/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** Tiles
*/

#include "Tiles.hpp"
#include "Client.hpp"
#include "Player.hpp"


bool Tiles::remove_specific_client(int player_num)
{
    //check if client is on this tile.

    for (auto it = players.begin(); it != players.end(); ++it) {
        if (auto sp = it->lock()) {
            if (sp->getId() == player_num) {
                players.erase(it);
                return true;
            }
        } else {
            it = players.erase(it);
            --it; //adjust iterator after erase
        }
    }
    return false;
}
