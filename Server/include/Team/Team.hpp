/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** Team
*/

#ifndef INCLUDED_TEAM_HPP
    #define INCLUDED_TEAM_HPP

#include "Egg.hpp"
#include <iostream>
#include <memory>
#include <vector>

class Team
{
    private:
    protected:
    public:
        Team(std::string name, int num_client_per_team)
        {
            this->name = name;
            this->spots_left = num_client_per_team;
        }
        ~Team() = default;

        int spots_left;
        std::string name;
        std::vector<std::shared_ptr<Egg>> eggs;

};


#endif
