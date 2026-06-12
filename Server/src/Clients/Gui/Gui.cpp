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
        send_message("suc\n");
        return;
    }
    std::vector<std::string> args;
    std::string arg;
    while (ss >> arg) {
        args.push_back(arg);
    }
    switch (it->second) {
        case GuiCommands::MSZ:
            msz(server);
            break;
        case GuiCommands::BCT:
            bct(server, args);
            break;
        case GuiCommands::MCT:
            mct(server);
            break;
        case GuiCommands::TNA:
            tna(server);
            break;
        case GuiCommands::PPO:
            ppo(server, args);
            break;
        case GuiCommands::PLV:
            plv(server, args);
            break;
        case GuiCommands::PIN:
            pin(server, args);
            break;
        case GuiCommands::SGT:
            sgt(server);
            break;
        case GuiCommands::SST:
            sst(server, args);
            break;
        default:
            send_message("suc\n");
            break;
    }
}
