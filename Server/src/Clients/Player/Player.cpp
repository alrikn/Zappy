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
        default:
            send_message("ko\n");
            break;
    }
}
