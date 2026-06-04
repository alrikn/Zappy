/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** Player
*/

#ifndef INCLUDED_PLAYER_HPP
    #define INCLUDED_PLAYER_HPP


#include "Client.hpp"
#include "Struct.hpp"

#include <array>
#include <string>
#include <vector>

typedef enum orientation{
    NORTH = 0,
    EAST = 1,
    SOUTH = 2,
    WEST = 3
} orientation_t;

class Subject;

class Player : public Client
{
    private:
        int player_id;
    protected:
    public:

        static int player_num; //this will be used to assign a unique number to each player, it will be incremented each time a new player is created.
        Player(Subject &subject, int control_fd = -1) : Client(PLAYER, subject, control_fd), player_id(++player_num) {};
        ~Player() = default;

        int getId() const { return player_id; }


        /*variables that are needed for the player*/

        std::array<int, 2> position; //x and y position of the player on the map
        orientation_t orientation; //the direction the player is facing (0 = north, 1 = east, 2 = south, 3 = west)
        int level = 0; //the level of the player
        std::string team_name; //the name of the team the player belongs to
        resources_t inventory = {10, 0, 0, 0, 0, 0, 0}; //the inventory of the player, it contains the number of each resource the player has


        Player& set_position(int x, int y) {position[0] = x;position[1] = y; return *this;}
        Player& set_orientation(orientation_t orientation){this->orientation = orientation; return *this;}
        Player& set_level(int level){this->level = level; return *this;}
        Player& set_team_name(std::string team_name){this->team_name = team_name; return *this;}
        Player& set_inventory(resources_t inventory){this->inventory = inventory; return *this;}

        //we do need to set up a behavioral observer pattern for the player,
        // because the player needs to be able to receive updates from the server,
        // and also send commands to the server.
        // so we need to be able to notify the player when certain events happen on the server,
        // and also be able to send commands to the server when the player wants to do something.


        void parse_command(const std::string command, Server &server) override;


        //all the player commands:
        void forward(Server &server);
        void right(Server &server);
        void left(Server &server);
        void look(Server &server);
        void inventory_handle(Server &server);

};


#endif
