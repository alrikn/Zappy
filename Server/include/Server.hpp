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
#include "Team.hpp"
#include <memory>
#include <vector>

class Server
{
    private:
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
        std::vector<std::vector<inventory_t>> map; //map of the game, each cell is like an inventory since it coins resources on that cell
        std::vector<std::shared_ptr<Client>> clients; //we will most likely need to seprate the clients into players and gui clients when processing, but for the interest of polling everything at once, we keep it together for now
        long long time_unit = 1000; //time unit in milliseconds (how long between each tick)
        long long tick = 0; //the current tick of the game, starts at 0 and increments by 1 every time_unit milliseconds

        std::vector<std::shared_ptr<Team>> teams; //the teams of the game.

        int port_number;

        /*server functions*/
        void run();


};


#endif
