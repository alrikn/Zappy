/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** Server
*/

#include "Server.hpp"
#include <csignal>
#include <cstring>
#include <memory>


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
    this->_map = std::vector<std::vector<Tiles>>(map_height, std::vector<Tiles>(map_width, Tiles()));
    //initialsie teams
    for (const std::string &team_name : team_names) {
        this->teams.push_back(std::make_shared<Team>(team_name, num_client_per_team));
    }
    _server_fd = set_up_server_socket(_port);
    add_fd(_server_fd);
}

void Server::handle_client_event(int client_fd)
{
    char buf[4096];
    ssize_t n = read(client_fd, buf, sizeof(buf));
    size_t pos;

    if (n <= 0) {
        //remove_client(client_fd); TODO implement
        return;
    }

    std::shared_ptr<Client> client = _clients[client_fd];
    client->ctrl_buffer.append(buf, n);
    pos = client->ctrl_buffer.find('\n');
    while (pos != std::string::npos) {
        std::string command = client->ctrl_buffer.substr(0, pos);
        client->ctrl_buffer.erase(0, pos + 1);
        // Process the command
        //TODO: implement logic to process command
        pos = client->ctrl_buffer.find('\n');
    }
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

void Server::poll_clients(int timeout)
{
    if (poll(_fds.data(), _fds.size(), timeout) < 0) {
        if (errno == EINTR) {
            // Interrupted by signal, likely shutdown request
            return;
        }
        perror("poll");
        return;
    }
    handle_event(); //handle events gets called if poll detected smth
}

