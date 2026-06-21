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

void Gui::gui_start(Server &server)
{
    msz(server);
    sgt(server);
    mct(server);
    tna(server);
    // send enw for all existing eggs (player_id = -1 for initial/unhatched eggs)
    for (const auto& team : server.getTeams())
        for (const auto& egg : team->eggs)
            //enw(egg->getId(), -1, egg->position[0], egg->position[1]); TODO: fix: enw
    // send pnw + pin + ebo for already-connected players
    for (const auto& [fd, client] : server._clients) {
        if (client->get_type() == client_type::PLAYER) {
            auto player = std::dynamic_pointer_cast<Player>(client);
            pnw(player);
            pin(server, {std::to_string(player->getId())});
            ebo(player->parent_egg_id);  // the egg this player hatched from
        }
    }
}


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
    std::cout << "Gui Com: " << verb << " args: ";
    for (const auto& a : args) {
        std::cout << a << " ";
    }
    std::cout << std::endl;
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
