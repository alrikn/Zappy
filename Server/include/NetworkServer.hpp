/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** NetworkServer
*/

#ifndef INCLUDED_NETWORKSERVER_HPP
    #define INCLUDED_NETWORKSERVER_HPP

#include "Client.hpp"
#include "SendMessageQueue.hpp"

#include <functional>
#include <memory>
#include <netinet/in.h>
#include <string>
#include <sys/poll.h>
#include <unordered_map>
#include <vector>

class NetworkServer
{
    private:
        static constexpr int MAX_PENDING_CLIENTS = 100;

        int create_server_socket();
        void bind_socket(int port, int server_fd);
        void socket_listen(int server_fd);
        int set_up_server_socket(int port);
        void add_fd(int fd);

        void accept_new_client();
        void handle_pending_client(int fd);
        void handle_client_event(int fd);
        void handle_event();

    public:
        int _port;
        int _server_fd;
        std::vector<pollfd> _fds;
        std::unordered_map<int, std::string> _pending_clients;
        std::unordered_map<int, std::shared_ptr<Client>> _clients;
        SendMessageQueue send_message_queue;

        // hooks wired by Server at construction time
        std::function<void(int fd, const std::string &team_name)> _on_team_name;
        std::function<void(int fd, const std::string &cmd)>       _on_command;
        std::function<void(int fd)>                               _on_disconnect;

        NetworkServer(int port);

        void poll_clients(int timeout);
        void add_client(std::shared_ptr<Client> client);
        void remove_connection(int fd);
};

#endif
