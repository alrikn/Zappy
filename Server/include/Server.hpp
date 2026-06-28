/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** Server
*/

#ifndef INCLUDED_SERVER_HPP
    #define INCLUDED_SERVER_HPP

#include "NetworkServer.hpp"
#include "Egg.hpp"
#include "Gui.hpp"
#include "Struct.hpp"
#include "Player.hpp"
#include "Team.hpp"
#include "Tiles.hpp"
#include "Subject.hpp"

#include <memory>
#include <vector>

class Server
{
    private:
        //_network MUST be declared before the reference aliases below
        NetworkServer _network;

        void finalize_client(int client_fd, std::string team_name);
        void remove_client(int client_fd);
        void free_team_slot(std::shared_ptr<Client> client);
        std::shared_ptr<Player> create_player(int client_fd, std::string team_name);
        std::shared_ptr<Gui> create_gui(int client_fd);
        void populate_map_resources();
        void game_tick();
        void advance_game();
        void respawn_resources();
        void drain_food(std::shared_ptr<Player> player,
            std::vector<std::shared_ptr<Player>> &to_kill);
        void step_player_action(std::shared_ptr<Player> player);
        void kill_player(std::shared_ptr<Player> player);

    public:
        Server(int port_number,
            int map_width,
            int map_height,
            std::vector<std::string> team_names,
            int num_client_per_team,
            long long trantorian_time_unit = 100);
        ~Server() = default;

        Subject _gui_subject;
        Subject _player_subject;
        std::vector<std::vector<Tiles>> _map;

        //reference aliases into _network
        std::unordered_map<int, std::shared_ptr<Client>> &_clients;
        SendMessageQueue &send_message_queue;

        long long time_unit = 1000;
        long long _freq = 100;
        long long tick = 0;
        long long _last_respawn_tick = 0;

        std::vector<std::shared_ptr<Team>> teams;

        long long client_num = 0;
        bool running = true;

        void run();

        bool move_player(Player &player, int x, int y);

        int getMapWidth() const { return _map[0].size(); }
        int getMapHeight() const { return _map.size(); }
        std::vector<std::shared_ptr<Team>> getTeams() const { return teams; }
        long long getTimeUnit() const { return time_unit; }
        long long getFreq() const { return _freq; }
};

#endif
