/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** Gui
*/

#ifndef INCLUDED_GUI_HPP
    #define INCLUDED_GUI_HPP

#include "Client.hpp"
#include <iostream>

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

};


#endif
