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
}
