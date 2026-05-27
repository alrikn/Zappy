/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** Server
*/

#ifndef INCLUDED_SERVER_HPP
    #define INCLUDED_SERVER_HPP

#include "Client.hpp"
#include "Struct.hpp"
#include "Player.hpp"
#include "Team.hpp"
#include <memory>
#include <unordered_map>
#include <vector>
#include <netinet/in.h>
#include <sys/poll.h>
#include <memory>

class Server
{
    private:
        /*client facing functions*/
        void accept_new_client();
        void add_client(std::shared_ptr<Client> client);
        void remove_client(int client_fd);
        void handle_client_event(int client_fd);
        void handle_event();
        /*server setup functions*/
        int create_server_socket();
        sockaddr_in bind_socket(int port, int server_fd);
        void socket_listen(int server_fd);
        int set_up_server_socket(int port);
        void add_fd(int fd);

        /*game functions*/
        std::shared_ptr<Player> create_player(int client_fd, std::string team_name);

    protected:
    public:
        Server(int port_number,
            int map_width,
            int map_height,
            std::vector<std::string> team_names,
            int num_client_per_team,
            long long trantorian_time_unit = 100);
        ~Server() = default;


        /*server variables*/
        //TODO: there might be a need to make its a shared ptr
        std::vector<std::vector<inventory_t>> _map; //map of the game, each cell is like an inventory since it coins resources on that cell
        std::unordered_map<int, std::shared_ptr<Client>> _clients; //map of all connected clients, keyed by client fd
        long long time_unit = 1000; //time unit in milliseconds (how long between each tick)
        long long tick = 0; //the current tick of the game, starts at 0 and increments by 1 every time_unit milliseconds

        std::vector<std::shared_ptr<Team>> teams; //the teams of the game.

        /*more technical variables*/
        int _port;
        int _server_fd;
        std::vector<pollfd> _fds; //list of all fd (including the server) that we can loop through

        /*server functions*/
        void run();


};


#endif
