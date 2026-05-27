/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** Server
*/

#include "Server.hpp"
#include <csignal>

extern volatile sig_atomic_t g_shutdown_requested;

Server::Server(int port_number,
            int map_width,
            int map_height,
            std::vector<std::string> team_names,
            int num_client_per_team,
            long long trantorian_time_unit)
{
    this->time_unit = (7 / trantorian_time_unit) / 1000; //that just how the pdf want it, divide by 1000 to make milliseconds

    this->tick = 0;
    this->_port = port_number;
    //we intialise the map with no resources.
    this->map = std::vector<std::vector<inventory_t>>(map_height, std::vector<inventory_t>(map_width, {0, 0, 0, 0, 0, 0, 0}));
    //initialsie teams
    for (const std::string &team_name : team_names) {
        this->teams.push_back(std::make_shared<Team>(team_name, num_client_per_team));
    }
    _server_fd = set_up_server_socket(_port);
    add_fd(_server_fd);
}

void Server::handle_event()
{
    for (size_t i = 0; i < _fds.size(); ++i) {
        if (!(_fds[i].revents & POLLIN)) //check if an event happened that we can read
            continue;

        if (_fds[i].fd == _server_fd) { //this means that there is a new client we need to acces
            accept_new_client();
        } else {//otherwise it means that the client sent us smth, so we read it
            handle_client_event(_fds[i].fd);
        }
    }
}

void Server::run()
{
    while (!g_shutdown_requested) {
        if (poll(_fds.data(), _fds.size(), -1) < 0) {
            if (errno == EINTR && g_shutdown_requested)
                break;
            perror("poll");
            break;
        }
        handle_event();
    }
}
