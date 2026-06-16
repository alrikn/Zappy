/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** Server
*/

#ifndef INCLUDED_SERVER_HPP
    #define INCLUDED_SERVER_HPP

#include "Client.hpp"
#include "Egg.hpp"
#include "Gui.hpp"
#include "Struct.hpp"
#include "Player.hpp"
#include "Team.hpp"
#include "Tiles.hpp"
#include "Subject.hpp"
#include <memory>
#include <unordered_map>
#include <vector>
#include <netinet/in.h>
#include <sys/poll.h>

class Server
{
    private:
        /*client facing functions*/
        void accept_new_client();
        void add_client(std::shared_ptr<Client> client);
        void remove_client(int client_fd);
        void handle_client_event(int client_fd);
        void handle_event();
        void free_team_slot(std::shared_ptr<Client> client);
        /*server setup functions*/
        int create_server_socket();
        sockaddr_in bind_socket(int port, int server_fd);
        void socket_listen(int server_fd);
        int set_up_server_socket(int port);
        void add_fd(int fd);

        /*game functions*/
        void poll_clients(int timeout);
        std::shared_ptr<Player> create_player(int client_fd, std::string team_name);
        std::shared_ptr<Gui> create_gui(int client_fd);
        void populate_map_resources();
        void game_tick();
        void advance_game();
        // advance_game helpers, split out so each does one thing (see game_loop.cpp)
        void respawn_resources();
        void drain_food(std::shared_ptr<Player> player,
            std::vector<std::shared_ptr<Player>> &to_kill);
        void step_player_action(std::shared_ptr<Player> player);
        void kill_player(std::shared_ptr<Player> player);

        /*observer behavioral pattern functions*/
        void attach(Client *client);
        void detach(Client *client);
        void notify();

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
        //main subject functions from the observer pattern:
        Subject _gui_subject; //the subject that all gui observe
        Subject _player_subject; //the subject that all players observe, we may not need this but it could be useful to inform the player of certain events (for example, the result of a command they sent to the server)
        //TODO: there might be a need to make its a shared ptr
        std::vector<std::vector<Tiles>> _map; //map of the game, each cell is like an inventory since it coins resources on that cell
        std::unordered_map<int, std::shared_ptr<Client>> _clients; //map of all connected clients, keyed by client fd
        long long time_unit = 1000;
        long long tick = 0;
        long long _last_respawn_tick = 0; //tick of the most recent resource respawn

        std::vector<std::shared_ptr<Team>> teams; //the teams of the game.

        long long client_num = 0; //increments every time a new client gets added
        /*more technical variables*/
        int _port;
        int _server_fd;
        std::vector<pollfd> _fds; //list of all fd (including the server) that we can loop through

        bool running = true;
        /*server functions*/
        void run();

        /*client helper functions*/

        void move_player(Player &player, int x, int y);
        void notify_gui(const std::string &message);

        int getMapWidth() const { return _map[0].size(); }
        int getMapHeight() const { return _map.size(); }
        std::vector<std::shared_ptr<Team>> getTeams() const { return teams; }
        long long getTimeUnit() const { return time_unit; }


};


#endif
