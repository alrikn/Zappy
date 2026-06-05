/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** Actions
*/

#include "Player.hpp"
#include "Server.hpp"
#include <string>
#include <tuple>
#include <vector>




void Player::inventory_handle()
{
    std::string response = "[";

    std::vector<std::tuple<std::string, int>> resources = give_resources_number(inventory);
    for (size_t i = 0; i < resources.size(); i++) {
        const auto &resource = resources[i];
        response += std::get<0>(resource) + " " + std::to_string(std::get<1>(resource));
        if (i < resources.size() - 1) {
            response += ", ";
        }
    }
    response += "]\n";
    send_message(response);
}

void Player::set_down_resource(Server &server, std::vector<std::string> args)
{
    std::string resource_name = args[0];
    int quantity = std::stoi(args[1]);

    if (resource_name == "food") {
        inventory.food -= quantity;
        server._map[position[1]][position[0]].resources.food += quantity;
    }
    if (resource_name == "linemate") {
        inventory.linemate -= quantity;
        server._map[position[1]][position[0]].resources.linemate += quantity;
    }
    if (resource_name == "deraumere") {
        inventory.deraumere -= quantity;
        server._map[position[1]][position[0]].resources.deraumere += quantity;
    }
    if (resource_name == "sibur") {
        inventory.sibur -= quantity;
        server._map[position[1]][position[0]].resources.sibur += quantity;
    }
    if (resource_name == "mendiane") {
        inventory.mendiane -= quantity;
        server._map[position[1]][position[0]].resources.mendiane += quantity;
    }
    if (resource_name == "phiras") {
        inventory.phiras -= quantity;
        server._map[position[1]][position[0]].resources.phiras += quantity;
    }
    if (resource_name == "thystame") {
        inventory.thystame -= quantity;
        server._map[position[1]][position[0]].resources.thystame += quantity;
    }
    send_message("ok\n");
}
