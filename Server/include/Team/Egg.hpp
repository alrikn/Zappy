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
        Egg(std::string team_name, std::vector<int> position) : team_name(team_name), position(position) {};

        ~Egg() = default;

        std::string team_name; //the name of the team the egg belongs to
        std::vector<int> position; //x and y position of the egg on the map

};


#endif
