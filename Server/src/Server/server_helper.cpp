/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** server_helper
*/

#include "Gui.hpp"
#include "Player.hpp"
#include "Server.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <unistd.h>

void Server::free_team_slot(std::shared_ptr<Client> client)
{
    std::shared_ptr<Player> player = std::dynamic_pointer_cast<Player>(client);
    if (!player)
        return;
    for (auto &team : teams) {
        if (team->name == player->team_name) {
            team->spots_left++;
            return;
        }
    }
}


/**
 * Funcs taken from bootstrap (i may move them around later)
 *
*/
int Server::create_server_socket()
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    //AF_INET : IPv4 protocol
    //SOCK_STREAM: TCP socket
    if (server_fd < 0) {
        perror("socket");
        throw std::runtime_error("socket");
    }

    int opt = 1;
    //what happens here:
    //when our server closes, the os keep the port in a state called TIME_WAIT for a few minutes
    //we are overriding that with our command SO_REUSEADDR to let us reeuse it immediately
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    return server_fd;

}

sockaddr_in Server::bind_socket(int port, int server_fd)
{
    sockaddr_in addr; //describing the stock addr


    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET; //must match what we used in sockeyt
    addr.sin_port = htons(port); //Host TO Network Short (apperently this changes the endian type to be whatever the network needs it to be (BE))
    addr.sin_addr.s_addr = INADDR_ANY; //listen to all local interface


    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) { //assigns name (name is Ip + PORT) to the socket (essentially attaches the socket to the port)
        //diff between port and socket:
        //socket is smth that can send and rcv data (sockets use ports)
        //port is just a number
        perror("bind");
        throw std::runtime_error("bind");
    }
    return addr;
}

void Server::socket_listen(int server_fd)
{
    if (listen(server_fd, 10) < 0) { //turns socket in passive listening socket (with a queue of 10)
        //this little thing makes it able to accept connection s and queue connection requests
        perror("listen");
        throw std::runtime_error("listen");
    }
}

int Server::set_up_server_socket(int port)
{
    int server_fd = create_server_socket();

    bind_socket(port, server_fd);

    socket_listen(server_fd);
    return server_fd;
}

void Server::add_fd(int fd)
{
    pollfd p;
    p.fd = fd;
    p.events = POLLIN; //tell me when smth is availabe to readd?
    p.revents = 0;
    _fds.push_back(p);
}

std::shared_ptr<Player> Server::create_player(int client_fd, std::string team_name)
{
    //first we check if team name is valid and if there is still a spot left in that team
    std::shared_ptr<Team> matched_team;

    for (const std::shared_ptr<Team> &team : teams) {
        if (team->name == team_name) {
            matched_team = team;
            break;
        }
    }
    if (!matched_team || matched_team->spots_left <= 0)
        return nullptr;
    matched_team->spots_left--;

    // hatch from a forked egg if one exists, otherwise random spawn
    std::vector<int> position;
    int egg_id = -1;
    if (!matched_team->eggs.empty()) {
        auto egg = matched_team->eggs.back();
        matched_team->eggs.pop_back();
        position = egg->position;
        egg_id = egg->getId();
    } else {
        position.push_back(rand() % _map[0].size());
        position.push_back(rand() % _map.size());
    }

    std::shared_ptr<Player> player = std::make_shared<Player>(_player_subject, client_fd);
    player->set_orientation(NORTH)
    .set_team_name(team_name)
    .set_level(1)
    .set_position(position[0], position[1]);

    player->parent_egg_id = egg_id;

    // add to initial tile so tile scanning (incantation, eject, look) finds the player
    _map[position[1]][position[0]].players.push_back(player);

    // protocol: line1 = remaining team slots, line2 = map dimensions (width height)
    std::string valid_message = std::to_string(matched_team->spots_left) + "\n" // CLIENT-NUM\n (the lsot line)
        + std::to_string(getMapWidth()) + " " + std::to_string(getMapHeight()) + "\n"; // X Y\n

    write(client_fd, valid_message.c_str(), valid_message.length());

    return player;
}

void Server::kill_player(std::shared_ptr<Player> player)
{
    player->send_message("dead\n");
    _map[player->position[1]][player->position[0]].remove_specific_client(player->getId());
    //send message to gui about player death
    _gui_subject.Notify([player](Client* c) {
        static_cast<Gui*>(c)->pdi(player);
    });
    remove_client(player->control_fd);
}

std::shared_ptr<Gui> Server::create_gui(int client_fd)
{
    std::shared_ptr<Gui> gui = std::make_shared<Gui>(_gui_subject, client_fd);
    _gui_subject.Attach(gui);
    //now we write a bunc of stuff to this new gui
    gui->gui_start(*this);
    return gui;
}

void Server::remove_client(int client_fd)
{
    auto it = _clients.find(client_fd);

    close(client_fd); //very important, because if we do not close somebody that has been accepted it leads to weird stuff
    if (it == _clients.end())
        return;

    it->second->RemoveMeFromList();
    send_message_queue.clear_messages_for_client(client_fd);
    free_team_slot(it->second);
    _clients.erase(it);

    _fds.erase(std::remove_if(_fds.begin(), _fds.end(),
        [client_fd](const pollfd &p) { return p.fd == client_fd; }), _fds.end());
}

void Server::add_client(std::shared_ptr<Client> client)
{
    _clients[client->control_fd] = client;
    client_num++;
}

void Server::accept_new_client()
{
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    int client_fd = accept(_server_fd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) {
        perror("accept");
        return;
    }

    if (_pending_clients.size() >= MAX_PENDING_CLIENTS) {
        std::cerr << "Max pending clients reached, rejecting new connection" << std::endl;
        close(client_fd);
        return;
    }

    add_fd(client_fd);
    write(client_fd, "WELCOME\n", 8);
    _pending_clients[client_fd] = "";
}

void Server::handle_pending_client(int client_fd)
{
    char buf[4096];
    ssize_t n = read(client_fd, buf, sizeof(buf));
    if (n <= 0) {
        _pending_clients.erase(client_fd);
        remove_client(client_fd);
        return;
    }

    _pending_clients[client_fd].append(buf, n);

    size_t pos = _pending_clients[client_fd].find('\n');
    if (pos == std::string::npos)
        return;

    std::string team_name = _pending_clients[client_fd].substr(0, pos + 1);
    _pending_clients.erase(client_fd);

    //strip \r\n to normalise
    while (!team_name.empty() && (team_name.back() == '\n' || team_name.back() == '\r'))
        team_name.pop_back();

    finalize_client(client_fd, team_name);
}

void Server::finalize_client(int client_fd, std::string team_name)
{
    if (team_name == "GRAPHIC") {
        std::shared_ptr<Gui> gui = create_gui(client_fd);
        add_client(gui);
    } else {
        std::shared_ptr<Player> player = create_player(client_fd, team_name);
        if (player) {
            int egg_id = player->parent_egg_id;
            add_client(player); // must be in _clients before pin searches for it
            _gui_subject.Notify([player, egg_id, this](Client* c) {
                auto gui = static_cast<Gui*>(c);
                gui->pnw(player);
                gui->pin(*this, {std::to_string(player->getId())});
                if (egg_id != -1)
                    gui->ebo(egg_id);
            });
        } else {
            remove_client(client_fd);
        }
    }
}

