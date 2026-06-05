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
#include <vector>


int Player::player_num = 0; //initialize the static player_num variable

std::vector<std::tuple<std::string, int>> Player::give_resources_number(resources_t resources)
{
    std::vector<std::tuple<std::string, int>> result;

    if (resources.food > 0)
        result.push_back(std::make_tuple("food", resources.food));
    if (resources.linemate > 0)
        result.push_back(std::make_tuple("linemate", resources.linemate));
    if (resources.deraumere > 0)
        result.push_back(std::make_tuple("deraumere", resources.deraumere));
    if (resources.sibur > 0)
        result.push_back(std::make_tuple("sibur", resources.sibur));
    if (resources.mendiane > 0)
        result.push_back(std::make_tuple("mendiane", resources.mendiane));
    if (resources.phiras > 0)
        result.push_back(std::make_tuple("phiras", resources.phiras));
    if (resources.thystame > 0)
        result.push_back(std::make_tuple("thystame", resources.thystame));
    return result;
}

std::vector<std::string> Player::give_resources_name(resources_t resources)
{
    std::vector<std::string> result;

    if (resources.deraumere > 0)
        result.push_back("deraumere");
    if (resources.sibur > 0)
        result.push_back("sibur");
    if (resources.mendiane > 0)
        result.push_back("mendiane");
    if (resources.phiras > 0)
        result.push_back("phiras");
    if (resources.thystame > 0)
        result.push_back("thystame");
    return result;
}

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
    std::vector<std::string> args; //we want everything except the first word to be in this vector, so we can easily access it when we need to
    std::string arg;

    while (ss >> arg) {
        args.push_back(arg);
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
        case LOOK:
            look(server);
            break;
        case INVENTORY:
            inventory_handle();
            break;
        case SET:
            set_down_resource(server, args);
            break;
        default:
            send_message("ko\n");
            break;
    }
}
