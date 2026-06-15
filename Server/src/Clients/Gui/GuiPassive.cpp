/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** GuiPassive
** This file is for all the functions that the server sends to the gui without explicit request from the gui.
** These functions will jnot handle throwing the message to evry gui connection, they only focus on themselves
*/

#include "Gui.hpp"
#include "Player.hpp"

//connection of a new player
//returns: pnw <player id> <x> <y> <orientation> <level> <team name>
void Gui::pnw(std::shared_ptr<Player> player)
{
    std::string result = "pnw";

    result += " " + std::to_string(player->getId());
    result += " " + std::to_string(player->getX());
    result += " " + std::to_string(player->getY());
    result += " " + std::to_string(player->getOrientation());
    result += " " + std::to_string(player->getLevel());
    result += " " + player->getTeamName();
    send_message(result);
}

//player expulsion
//returns: pex <player id>
void Gui::pex(std::shared_ptr<Player> player)
{
    std::string result = "pex";

    result += " " + std::to_string(player->getId());
    send_message(result);
}
