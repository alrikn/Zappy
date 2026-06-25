/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** NetworkServer
*/

#include "NetworkServer.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <memory>
#include <unistd.h>

NetworkServer::NetworkServer(int port) : _port(port), _server_fd(-1)
{
    _server_fd = set_up_server_socket(port);
    add_fd(_server_fd);
}

//socket setup

int NetworkServer::create_server_socket()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        throw std::runtime_error("socket");
    }
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    return fd;
}

void NetworkServer::bind_socket(int port, int server_fd)
{
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        throw std::runtime_error("bind");
    }
}

void NetworkServer::socket_listen(int server_fd)
{
    if (listen(server_fd, 10) < 0) {
        perror("listen");
        throw std::runtime_error("listen");
    }
}

int NetworkServer::set_up_server_socket(int port)
{
    int fd = create_server_socket();
    bind_socket(port, fd);
    socket_listen(fd);
    return fd;
}

void NetworkServer::add_fd(int fd)
{
    pollfd p;
    p.fd = fd;
    p.events = POLLIN;
    p.revents = 0;
    _fds.push_back(p);
}

//client lifecycle

void NetworkServer::add_client(std::shared_ptr<Client> client)
{
    _clients[client->get_fd()] = client;
}

void NetworkServer::remove_connection(int fd)
{
    close(fd);
    send_message_queue.clear_messages_for_client(fd);
    _clients.erase(fd);
    _fds.erase(std::remove_if(_fds.begin(), _fds.end(),
        [fd](const pollfd &p) { return p.fd == fd; }), _fds.end());
}

//polling & event dispatch

void NetworkServer::accept_new_client()
{
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(_server_fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0) {
        perror("accept");
        return;
    }
    if (static_cast<int>(_pending_clients.size()) >= MAX_PENDING_CLIENTS) {
        std::cerr << "Max pending clients reached, rejecting new connection" << std::endl;
        close(client_fd);
        return;
    }
    add_fd(client_fd);
    int n = write(client_fd, "WELCOME\n", 8);
    if (n < 8) {
        perror("write");
    }
    _pending_clients[client_fd] = "";
}

void NetworkServer::handle_pending_client(int client_fd)
{
    char buf[4096];
    ssize_t n = read(client_fd, buf, sizeof(buf));
    if (n <= 0) {
        _pending_clients.erase(client_fd);
        _on_disconnect(client_fd);
        return;
    }

    _pending_clients[client_fd].append(buf, n);

    size_t pos = _pending_clients[client_fd].find('\n');
    if (pos == std::string::npos)
        return;

    std::string team_name = _pending_clients[client_fd].substr(0, pos + 1);
    _pending_clients.erase(client_fd);
    while (!team_name.empty() && (team_name.back() == '\n' || team_name.back() == '\r'))
        team_name.pop_back();

    _on_team_name(client_fd, team_name);
}

void NetworkServer::handle_client_event(int client_fd)
{
    char buf[4096];
    ssize_t n = read(client_fd, buf, sizeof(buf));

    if (n <= 0) {
        _on_disconnect(client_fd);
        return;
    }

    auto client_it = _clients.find(client_fd);
    if (client_it == _clients.end()) {
        std::cerr << "Error: client not found for fd " << client_fd << std::endl;
        _on_disconnect(client_fd);
        return;
    }
    auto client = client_it->second;
    client->write_to_buffer(buf, n);
    std::string command;
    while (client->read_line(command)) { //command is now updated with next line
        _on_command(client_fd, command);
        //command handler may have removed the client (e.g. disconnect mid batch)
        if (_clients.find(client_fd) == _clients.end())
            return;
    }
}

void NetworkServer::handle_event()
{
    for (size_t i = 0; i < _fds.size(); ++i) {
        if (!(_fds[i].revents & POLLIN))
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
                i--;
        }
    }
}

void NetworkServer::poll_clients(int timeout)
{
    if (poll(_fds.data(), _fds.size(), timeout) < 0) {
        if (errno == EINTR)
            return;
        perror("poll");
        return;
    }
    handle_event();
}
