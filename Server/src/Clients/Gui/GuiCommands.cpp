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
    send_message("msz " + std::to_string(server.getMapWidth()) + " " + std::to_string(server.getMapHeight()) + "\n");
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
    std::string result;

    for (int x = 0; x < server.getMapWidth(); x++) {
        for (int y = 0; y < server.getMapHeight(); y++) {
            Tiles tile = server._map[y][x];
            result += "bct " + std::to_string(x) + " " + std::to_string(y);
            for (size_t i = 0; i < static_cast<size_t>(Resource::Count); i++) {
                result += " " + std::to_string(tile.inventory.resources[i]);
            }
            result += "\n";
        }
    }
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

void Gui::ppo(Server &server, std::vector<std::string> args)
{
    if (args.size() != 1) {
        send_message("suc\n");
        return;
    }
    std::string id_str = args[0];
    if (!id_str.empty() && id_str[0] == '#') id_str = id_str.substr(1);
    int player_id = std::stoi(id_str);

    for (const auto& [fd, client] : server._clients) {
        if (client->get_type() == client_type::PLAYER) {
            auto player = std::dynamic_pointer_cast<Player>(client);
            if (player->getId() == player_id) {
                std::string result = "ppo #" + std::to_string(player_id) + " " + std::to_string(player->position[0]) + " " + std::to_string(player->position[1]) + " " + std::to_string(static_cast<int>(player->orientation)) + "\n";
                send_message(result);
                return;
            }
        }
    }
    send_message("suc\n");
}

void Gui::plv(Server &server, std::vector<std::string> args)
{
    if (args.size() != 1) {
        send_message("suc\n");
        return;
    }
    std::string id_str = args[0];
    if (!id_str.empty() && id_str[0] == '#') id_str = id_str.substr(1);
    int player_id = std::stoi(id_str);

    for (const auto& [fd, client] : server._clients) {
        if (client->get_type() == client_type::PLAYER) {
            auto player = std::dynamic_pointer_cast<Player>(client);
            if (player->getId() == player_id) {
                std::string result = "plv #" + std::to_string(player_id) + " " + std::to_string(player->level) + "\n";
                send_message(result);
                return;
            }
        }
    }
    send_message("suc\n");
}

void Gui::pin(Server &server, std::vector<std::string> args)
{
    if (args.size() != 1) {
        send_message("suc\n");
        return;
    }
    std::string id_str = args[0];
    if (!id_str.empty() && id_str[0] == '#') id_str = id_str.substr(1);
    int player_id = std::stoi(id_str);

    for (const auto& [fd, client] : server._clients) {
        if (client->get_type() == client_type::PLAYER) {
            auto player = std::dynamic_pointer_cast<Player>(client);
            if (player->getId() == player_id) {
                std::string result = "pin #" + std::to_string(player_id) + " " + std::to_string(player->position[0]) + " " + std::to_string(player->position[1]);
                for (size_t i = 0; i < static_cast<size_t>(Resource::Count); i++) {
                    result += " " + std::to_string(player->inventory.resources[i]);
                }
                result += "\n";
                send_message(result);
                return;
            }
        }
    }
    send_message("suc\n");
}

void Gui::sgt(Server &server)
{
    std::string result = "sgt " + std::to_string(server.getFreq()) + "\n";

    send_message(result);
}

void Gui::sst(Server &server, std::vector<std::string> args)
{
    if (args.size() != 1) {
        send_message("suc\n");
        return;
    }
    long long freq = std::stoll(args[0]);
    if (freq < 1) {
        send_message("suc\n");
        return;
    }
    server._freq = freq;
    server.time_unit = static_cast<long long>(1000.0 / freq);
    std::string result = "sst " + std::to_string(server.getFreq()) + "\n";
    send_message(result);
}
