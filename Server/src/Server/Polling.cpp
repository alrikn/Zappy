/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** Server
*/

#include "Server.hpp"
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <memory>


Server::Server(int port_number,
            int map_width,
            int map_height,
            std::vector<std::string> team_names,
            int num_client_per_team,
            long long trantorian_time_unit)
{
    this->_freq = trantorian_time_unit;
    this->time_unit = (1000.0 / trantorian_time_unit); //time unit is in milliseconds

    if (time_unit < 1) {
        throw std::runtime_error("Invalid time unit: " + std::to_string(time_unit));
    }
    std::cout << "time unit: " << time_unit << std::endl;
    this->tick = 0;
    this->_port = port_number;
    //we intialise the map with no resources.
    this->_map = std::vector<std::vector<Tiles>>(map_height, std::vector<Tiles>(map_width, Tiles()));
    //initialsie teams
    for (const std::string &team_name : team_names) {
        this->teams.push_back(std::make_shared<Team>(team_name, num_client_per_team));
    }
    // create initial eggs for each team slot (random positions, no parent player)
    for (auto &team : this->teams) {
        for (int i = 0; i < num_client_per_team; i++) {
            int x = rand() % map_width;
            int y = rand() % map_height;
            auto egg = std::make_shared<Egg>(team->name, std::vector<int>{x, y});
            team->eggs.push_back(egg);
        }
    }
    _server_fd = set_up_server_socket(_port);
    add_fd(_server_fd);
    populate_map_resources(); //seed the floor so the map isnt empty at startup
}

void Server::handle_client_event(int client_fd)
{
    char buf[4096];
    ssize_t n = read(client_fd, buf, sizeof(buf));
    size_t pos;

    if (n <= 0) {
        remove_client(client_fd);
        return;
    }

    auto client_it = _clients.find(client_fd);
    if (client_it == _clients.end()) {
        std::cout << "Error: client not found for fd " << client_fd << std::endl;
        remove_client(client_fd);
        return;
    }
    auto client = client_it->second;
    client->get_buffer().append(buf, n);
    pos = client->get_buffer().find('\n');
    while (pos != std::string::npos) {
        std::string command = client->get_buffer().substr(0, pos);
        client->get_buffer().erase(0, pos + 1);
        client->parse_command(command, *this);
        pos = client->get_buffer().find('\n');
    }
}

void Server::handle_event()
{
    for (size_t i = 0; i < _fds.size(); ++i) {
        if (!(_fds[i].revents & POLLIN)) //check if an event happened that we can read
            continue;

        if (_fds[i].fd == _server_fd) {
            accept_new_client();
        } else {
            size_t prev_size = _fds.size();
            if (_pending_clients.count(_fds[i].fd))
                handle_pending_client(_fds[i].fd);
            else
                handle_client_event(_fds[i].fd);
            if (_fds.size() < prev_size && i > 0)
                i--; // fd was removed during handling, recheck this index
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
