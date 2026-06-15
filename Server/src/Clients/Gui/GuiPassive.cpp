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
    result += "\n";
    send_message(result);
}

//player expulsion
//returns: pex <player id>
void Gui::pex(std::shared_ptr<Player> player)
{
    std::string result = "pex";

    result += " " + std::to_string(player->getId());
    result += "\n";
    send_message(result);
}

//player broadcast
//returns: pbc <player id> <message>
void Gui::pbc(std::shared_ptr<Player> player, std::string message)
{
    std::string result = "pbc";

    result += " " + std::to_string(player->getId());
    result += " " + message;
    result += "\n";
    send_message(result);
}

//player incantation start
//returns: pic <x> <y> <level> <array of player ids> (starts with the player that started the incantation)
void Gui::pic(int incantaion_level, std::vector<std::shared_ptr<Player>> players)
{
    std::string result = "pic";

    result += " " + std::to_string(players[0]->getX());
    result += " " + std::to_string(players[0]->getY());
    result += " " + std::to_string(incantaion_level);
    for (const auto& player : players) {
        result += " " + std::to_string(player->getId());
    }
    result += "\n";
    send_message(result);
}

//player incantation end
//returns: pie <x> <y> <result> (result is 0 if the incantation failed, 1 if it succeeded)
void Gui::pie(int x, int y, bool succeded)
{
    std::string result = "pie";

    result += " " + std::to_string(x);
    result += " " + std::to_string(y);
    result += " " + std::to_string(succeded ? 1 : 0);
    result += "\n";
    send_message(result);
}
