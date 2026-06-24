/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** server_helper
*/

#include "Gui.hpp"
#include "Player.hpp"
#include "Server.hpp"
#include "Tiles.hpp"

#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <unistd.h>

//team / slot helpers

void Server::free_team_slot(std::shared_ptr<Client> client)
{
    auto player = std::dynamic_pointer_cast<Player>(client);
    if (!player)
        return;
    for (auto &team : teams) {
        if (team->name == player->team_name) {
            team->spots_left++;
            return;
        }
    }
}

//client removal

void Server::remove_client(int client_fd)
{
    auto it = _clients.find(client_fd);
    if (it != _clients.end()) {
        auto player = std::dynamic_pointer_cast<Player>(it->second);
        if (player) {
            _map[player->position[1]][player->position[0]].remove_specific_client(player->getId());
            _gui_subject.Notify([player](Client *c) {
                static_cast<Gui *>(c)->pdi(player);
            });
        }
        it->second->RemoveMeFromList();
        free_team_slot(it->second);
    }
    _network.remove_connection(client_fd);
}

//player / gui factory

std::shared_ptr<Player> Server::create_player(int client_fd, std::string team_name)
{
    std::shared_ptr<Team> matched_team;
    for (const auto &team : teams) {
        if (team->name == team_name) {
            matched_team = team;
            break;
        }
    }
    if (!matched_team || matched_team->spots_left <= 0)
        return nullptr;
    matched_team->spots_left--;

    std::vector<int> position;
    int egg_id = -1;
    if (!matched_team->eggs.empty()) {
        auto egg = matched_team->eggs.back();
        matched_team->eggs.pop_back();
        position = egg->position;
        egg_id = egg->getId();
    } else {
        position.push_back(rand() % static_cast<int>(_map[0].size()));
        position.push_back(rand() % static_cast<int>(_map.size()));
    }

    auto player = std::make_shared<Player>(_player_subject, client_fd);
    player->set_orientation(NORTH)
        .set_team_name(team_name)
        .set_level(1)
        .set_position(position[0], position[1]);
    player->parent_egg_id = egg_id;

    _map[position[1]][position[0]].players.push_back(player);

    //protocol: slots remaining, then map dimensions
    std::string msg = std::to_string(matched_team->spots_left) + "\n"
        + std::to_string(getMapWidth()) + " " + std::to_string(getMapHeight()) + "\n";
    send_message_queue.add_message(*this, client_fd, msg, 0);
    return player;
}

std::shared_ptr<Gui> Server::create_gui(int client_fd)
{
    auto gui = std::make_shared<Gui>(_gui_subject, client_fd);
    _gui_subject.Attach(gui);
    gui->gui_start(*this);
    return gui;
}

//connection finalization

void Server::finalize_client(int client_fd, std::string team_name)
{
    if (team_name == "GRAPHIC") {
        auto gui = create_gui(client_fd);
        _network.add_client(gui);
        client_num++;
    } else {
        auto player = create_player(client_fd, team_name);
        if (player) {
            int egg_id = player->parent_egg_id;
            _network.add_client(player);
            client_num++;
            _gui_subject.Notify([player, egg_id, this](Client *c) {
                auto gui = static_cast<Gui *>(c);
                gui->pnw(player);
                gui->pin(*this, {std::to_string(player->getId())});
                if (egg_id != -1)
                    gui->ebo(egg_id);
            });
        } else {
            remove_client(client_fd);
        }
    }
}

//in game player ops

void Server::kill_player(std::shared_ptr<Player> player)
{
    player->send_message("dead\n");
    remove_client(player->get_fd());
}
