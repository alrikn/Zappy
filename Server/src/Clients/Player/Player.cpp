/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** Player
*/

#include "Player.hpp"
#include "Server.hpp"
#include "Parse.hpp"
#include <sstream>


int Player::player_num = 0; //initialize the static player_num variable




void Player::parse_command(const std::string raw, Server &server)
{
    std::istringstream ss(raw);
    std::string verb;
    ss >> verb;

    auto it = ClientCommandMap.find(verb);
    if (it == ClientCommandMap.end()) {
        send_message("ko\n");
        return;
    }
    switch (it->second) {
        case FORWARD:
            move_forward(server);
            break;
        case RIGHT:
            turn_right();
            break;
        case LEFT:
            turn_left();
            break;
        default:
            send_message("ko\n");
            break;
    }
}
