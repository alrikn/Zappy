/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** Server
*/

#include "Server.hpp"


Server::Server(int port_number,
            int map_width,
            int map_height,
            std::vector<std::string> team_names,
            int num_client_per_team,
            long long trantorian_time_unit)
{
    this->time_unit = (7 / trantorian_time_unit) / 1000; //that just how the pdf want it, divide by 1000 to make milliseconds

    this->tick = 0;
    this->port_number = port_number;
    //we intialise the map with no resources.
    this->map = std::vector<std::vector<inventory_t>>(map_height, std::vector<inventory_t>(map_width, {0, 0, 0, 0, 0, 0, 0}));
    //initialsie teams
    for (const std::string &team_name : team_names) {
        this->teams.push_back(std::make_shared<Team>(team_name, num_client_per_team));
    }



}
