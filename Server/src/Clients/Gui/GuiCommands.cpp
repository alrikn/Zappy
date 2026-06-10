/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** GuiCommands
*/

#include "Gui.hpp"
#include "Server.hpp"

void Gui::msz(Server &server)
{
    send_message("msz " + std::to_string(server.getMapHeight()) + " " + std::to_string(server.getMapWidth()) + "\n");
}

void Gui::bct(Server &server, std::vector<std::string> args)
{
    if (args.size() != 2) {
        send_message("suc\n");
        return;
    }
    int x = std::stoi(args[0]);
    int y = std::stoi(args[1]);
    if (x < 0 || x >= server.getMapWidth() || y < 0 || y >= server.getMapHeight()) {
        send_message("suc\n");
        return;
    }
    Tiles tile = server._map[y][x];
    std::string result = "bct " + std::to_string(x) + " " + std::to_string(y);
    for (size_t i = 0; i < static_cast<size_t>(Resource::Count); i++) {
        result += " " + std::to_string(tile.inventory.resources[i]);
    }
    result += "\n";
    send_message(result);
}

void Gui::mct(Server &server)
{
    std::string result = "bct"; //not sure if i'm supposesd to send bct every line or just at beginning

    for (int y = 0; y < server.getMapHeight(); y++) {
        for (int x = 0; x < server.getMapWidth(); x++) {
            Tiles tile = server._map[y][x];
            result += " " + std::to_string(x) + " " + std::to_string(y);
            for (size_t i = 0; i < static_cast<size_t>(Resource::Count); i++) {
                result += " " + std::to_string(tile.inventory.resources[i]);
            }
            result += "\n";
        }
    }
    //result += "\n";
    send_message(result);
}

void Gui::tna(Server &server)
{
    std::string result;

    for (const auto& team : server.getTeams()) {
        result += "tna";
        result += " " + team->name;
        result += "\n";
    }
    send_message(result);
}