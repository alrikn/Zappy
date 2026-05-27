/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** server_helper
*/

#include "Server.hpp"
#include <cstring>


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
