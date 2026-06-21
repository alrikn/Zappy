/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** Egg
*/

#ifndef INCLUDED_EGG_HPP
    #define INCLUDED_EGG_HPP

#include <iostream>
#include <vector>

class Egg
{
    private:
    protected:
    public:
        Egg(std::string team_name, std::vector<int> position)
            : id(++egg_counter), team_name(team_name), position(position) {}

        ~Egg() = default;

        inline static int egg_counter = 0;

        int id;
        int parent_player_id = -1; //the id of the player that laid the egg, -1 if the egg was not laid by a player
        std::string team_name;
        std::vector<int> position;

        /*getters*/
        int getId() const { return id; }
        std::string getTeamName() const { return team_name; }
        std::vector<int> getPosition() const { return position; }

};


#endif
