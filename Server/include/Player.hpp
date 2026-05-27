/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** Player
*/

#ifndef INCLUDED_PLAYER_HPP
    #define INCLUDED_PLAYER_HPP


#include "Client.hpp"

#include <string>
#include <vector>

typedef struct inventory
{
    int food; //food also represents lifespan of the player, if it reaches 0 the player dies
    int linemate;
    int deraumere;
    int sibur;
    int mendiane;
    int phiras;
    int thystame;
} inventory_t;

class Player : public Client
{
    private:
    protected:
    public:
        Player();
        ~Player() = default;


    /*variables that are needed for the player*/

    std::vector<int> position; //x and y position of the player on the map
    int orientation; //the direction the player is facing (0 = north, 1 = east, 2 = south, 3 = west)
    int level; //the level of the player
    std::string team_name; //the name of the team the player belongs to
    inventory_t inventory; //the inventory of the player


};


#endif
