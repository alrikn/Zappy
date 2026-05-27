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

#include <string>
#include <vector>

typedef enum orientation{
    NORTH = 0,
    EAST = 1,
    SOUTH = 2,
    WEST = 3
} orientation_t;

class Player : public Client
{
    private:
    protected:
    public:
        Player(int control_fd = -1) : Client(control_fd, PLAYER) {};
        ~Player() = default;


    /*variables that are needed for the player*/

    std::vector<int> position; //x and y position of the player on the map
    orientation_t orientation; //the direction the player is facing (0 = north, 1 = east, 2 = south, 3 = west)
    int level = 0; //the level of the player
    std::string team_name; //the name of the team the player belongs to
    inventory_t inventory = {10, 0, 0, 0, 0, 0, 0}; //the inventory of the player, it contains the number of each resource the player has


    Player& set_position(int x, int y) {position[0] = x;position[1] = y; return *this;}
    Player& set_orientation(orientation_t orientation){this->orientation = orientation; return *this;}
    Player& set_level(int level){this->level = level; return *this;}
    Player& set_team_name(std::string team_name){this->team_name = team_name; return *this;}
    Player& set_inventory(inventory_t inventory){this->inventory = inventory; return *this;}

};


#endif
