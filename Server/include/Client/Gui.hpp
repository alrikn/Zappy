/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** Gui
*/

#ifndef INCLUDED_GUI_HPP
    #define INCLUDED_GUI_HPP

#include "Client.hpp"
#include "Struct.hpp"
#include <iostream>
#include <vector>

class Gui : public Client
{
    private:
    protected:
    public:
        Gui(Subject &subject, int control_fd = -1) : Client(GUI, subject, control_fd) {};
        ~Gui() = default;

        /*
        gui a bit of a weird one, beacause it needs to be able to request data from the server, and also receive data from the server.

        I'm thinking of implementing a behavioral observer pattern,
        where the gui can subscribe to certain events on the server,
        and the server will notify the gui when those events happen.

        */

        void parse_command(const std::string command, Server &server) override;

        /*these are all the commands that the server can receive from the gui*/

        void msz(Server &server); //map size
        void bct(Server &server, std::vector<std::string> args); //content of a tile
        void mct(Server &server); //content of the map (all tiles)
        void tna(Server &server); //team names
        void ppo(Server &server, std::vector<std::string> args); //player position
        void plv(Server &server, std::vector<std::string> args); //player level
        void pin(Server &server, std::vector<std::string> args); //player inventory
        void sgt(Server &server); //server time unit (set gui time unit to the server time unit)
        void sst(Server &server, std::vector<std::string> args); //set server time unit (set server time unit to the gui time unit)

};


#endif
