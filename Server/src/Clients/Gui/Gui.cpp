/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** Gui
*/


#include "Gui.hpp"
#include "Server.hpp"
#include "Parse.hpp"
#include <sstream>


void Gui::parse_command(const std::string raw, Server &server)
{
    std::istringstream ss(raw);
    std::string verb;
    ss >> verb;

    auto it = GuiCommandMap.find(verb);
    if (it == GuiCommandMap.end()) {
        send_message("ko\n");
        return;
    }
    switch (it->second) {
        default:
            send_message("ko\n");
            break;
    }
}
