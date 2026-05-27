/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** Team
*/

#ifndef INCLUDED_TEAM_HPP
    #define INCLUDED_TEAM_HPP

#include <iostream>

class Team
{
    private:
    protected:
    public:
        Team();
        ~Team() = default;

        int spots_left; //the number of players that can still join the team, if it reaches 0 no more players can join the team
        std::string name; //the name of the team

};


#endif
