/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** server_helper
*/

#include "Server.hpp"
#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <unistd.h>

static void free_team_slot(std::vector<std::shared_ptr<Team>> &teams, std::shared_ptr<Client> client)
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
    p.events = POLLIN; //tell me when smth is availabe to read?
    p.revents = 0;
    _fds.push_back(p);
}

std::shared_ptr<Player> Server::create_player(int client_fd, std::string team_name)
{
    //first we check if team name is valid and if there is still a spot left in that team
    bool team_found = false;
    for (const std::shared_ptr<Team> &team : teams)
        if (team->name == team_name) {
            team_found = true;
            if (team->spots_left <= 0)
                return nullptr;
        }
    if (!team_found)
        return nullptr;
    //now we create the player. we give him a random non occupied position

    std::vector<int> position; //= random_unncoupied_position(team_name);

    std::shared_ptr<Player> player = std::make_shared<Player>(_player_subject, client_fd);

    player->set_orientation(NORTH)
    .set_team_name(team_name)
    .set_position(position[0], position[1]);

    //now we write to the client that it has been succesful
    std::string valid_message = std::to_string(client_num) + "\n" + std::to_string(position[0]) + " " + std::to_string(position[1]) + "\n";

    write(client_fd, valid_message.c_str(), valid_message.length());
    return player;
}

void Server::remove_client(int client_fd)
{
    auto it = _clients.find(client_fd);
    if (it == _clients.end())
        return;

    it->second->RemoveMeFromList();
    free_team_slot(teams, it->second);
    _clients.erase(it);

    _fds.erase(std::remove_if(_fds.begin(), _fds.end(),
        [client_fd](const pollfd &p) { return p.fd == client_fd; }), _fds.end());

    close(client_fd);
}

void Server::add_client(std::shared_ptr<Client> client)
{
    _clients[client->control_fd] = client;
    client_num++;
}

void Server::accept_new_client()
{
    //whats going on:
    //Takes the first waiting client from the queue
    //Creates a new socket dedicated to that client
    //Returns a new file descriptor
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    //accept removes the request(client) from the queue
    //and a new socket is created, and from all this we got a  new file descriptor
    int client_fd = accept(_server_fd, (struct sockaddr*)&client_addr, &client_len);

    add_fd(client_fd);

    //we now need to check if they are a gui or a player
    write(client_fd, "WELCOME\n", 8); //we send them the welcome message
    //now we wait for reply:
    char buffer[1024];
    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read < 0) {
        perror("read");
        close(client_fd);
        return;
    }
    buffer[bytes_read] = '\0'; //null terminate the buffer to make it a
    if (strcmp(buffer, "GRAPHIC\n") == 0) {
        //they are a gui
        //std::shared_ptr<Client> client = std::make_shared<Client>();
        //client->control_fd = client_fd;
        //add_client(client);
    } else {
        //they are a player, we need to check if the team they want is a valid one, and if there is still a spot left in that team
        std::string team_name(buffer);
        team_name.pop_back(); //remove the newline character
        std::shared_ptr<Player> player = create_player(client_fd, team_name);
        if (player) {
            add_client(player);
        } else {
            //invalid team or no spot left, we disconnect the client
            //write(client_fd, "ko\n", 3);
            close(client_fd);
        }
    }

}

