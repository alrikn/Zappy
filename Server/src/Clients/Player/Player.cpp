/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** Player
*/

#include "Player.hpp"
#include "Server.hpp"
#include "Parse.hpp"
#include "Struct.hpp"
#include <iostream>
#include <sstream>
#include <vector>


int Player::player_num = 0; //initialize the static player_num variable


Player::Player(Subject &subject, int control_fd) :Client(PLAYER, subject, control_fd), player_id(player_num++)
{
    inventory.resources[static_cast<size_t>(Resource::Food)] = 10;
    orientation = NORTH; //initialize orientation to NORTH
    position[0] = 0;
    position[1] = 0;
}

void Player::command_failed(Server &server, PlayerCommands verb)
{
    std::string response = "ko\n";
    server.send_message_queue.add_message(server, get_fd(), response);

    std::cout << "Player " << player_id << " command failed: " << ClientCommandReverseMap.at(verb) << std::endl;
}

std::vector<std::tuple<std::string, int>> Player::give_resources_number(const Inventory &inventory)
{
    std::vector<std::tuple<std::string, int>> result;

    if (inventory.resources[idx(Resource::Food)] > 0)
        result.push_back(std::make_tuple("food", inventory.resources[idx(Resource::Food)]));
    if (inventory.resources[idx(Resource::Linemate)] > 0)
        result.push_back(std::make_tuple("linemate", inventory.resources[idx(Resource::Linemate)]));
    if (inventory.resources[idx(Resource::Deraumere)] > 0)
        result.push_back(std::make_tuple("deraumere", inventory.resources[idx(Resource::Deraumere)]));
    if (inventory.resources[idx(Resource::Sibur)] > 0)
        result.push_back(std::make_tuple("sibur", inventory.resources[idx(Resource::Sibur)]));
    if (inventory.resources[idx(Resource::Mendiane)] > 0)
        result.push_back(std::make_tuple("mendiane", inventory.resources[idx(Resource::Mendiane)]));
    if (inventory.resources[idx(Resource::Phiras)] > 0)
        result.push_back(std::make_tuple("phiras", inventory.resources[idx(Resource::Phiras)]));
    if (inventory.resources[idx(Resource::Thystame)] > 0)
        result.push_back(std::make_tuple("thystame", inventory.resources[idx(Resource::Thystame)]));
    return result;
}

std::vector<std::string> Player::give_resources_name(const Inventory &inventory)
{
    std::vector<std::string> result;

    if (inventory.resources[idx(Resource::Food)] > 0)
        result.push_back("food");
    if (inventory.resources[idx(Resource::Linemate)] > 0)
        result.push_back("linemate");
    if (inventory.resources[idx(Resource::Deraumere)] > 0)
        result.push_back("deraumere");
    if (inventory.resources[idx(Resource::Sibur)] > 0)
        result.push_back("sibur");
    if (inventory.resources[idx(Resource::Mendiane)] > 0)
        result.push_back("mendiane");
    if (inventory.resources[idx(Resource::Phiras)] > 0)
        result.push_back("phiras");
    if (inventory.resources[idx(Resource::Thystame)] > 0)
        result.push_back("thystame");
    return result;
}

void Player::parse_command(const std::string raw, Server &server)
{
    (void)server;
    std::istringstream ss(raw);
    std::string verb;
    ss >> verb;

    auto it = ClientCommandMap.find(verb);
    if (it == ClientCommandMap.end()) {
        send_message("ko\n"); //bad/unknown command is answered immediately
        return;
    }
    std::vector<std::string> args; //we want everything except the first word to be in this vector, so we can easily access it when we need to
    std::string arg;

    std::cout << "Player " << player_id << " command: " << verb << std::endl;

    while (ss >> arg) {
        args.push_back(arg);
    }
    //subject: a client can buffer up to 10 requests, anything over is dropped
    //we only enqueue here, the game loop executes it once its time has elapsed
    if (cmd_queue.size() >= 10)
        return;
    cmd_queue.push_back({it->second, args});
}


void Player::execute_command(PlayerCommands verb, std::vector<std::string> args,
    Server &server)
{
    switch (verb) {
        case FORWARD:
            move_forward(server);
            break;
        case RIGHT:
            turn_right(server);
            break;
        case LEFT:
            turn_left(server);
            break;
        case LOOK:
            look(server);
            break;
        case INVENTORY:
            inventory_handle(server);
            break;
        case SET:
            set_down_resource(server, args);
            break;
        case TAKE:
            take_resource(server, args);
            break;
        case EJECT:
            eject(server);
            break;
        case BROADCAST:
            broadcast(server, args);
            break;
        case INCANTATION:
            incantation_start(server);
            break;
        case FORK:
            fork(server);
            break;
        case CONNECT_NBR:
            connect_nbr(server);
            break;
    }
}
