/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** Actions
*/

#include "Player.hpp"
#include "Server.hpp"
#include "Gui.hpp"
#include <string>
#include <tuple>
#include <vector>




void Player::inventory_handle()
{
    std::string response = "[";

    std::vector<std::tuple<std::string, int>> resources = give_resources_number(inventory);
    for (size_t i = 0; i < resources.size(); i++) {
        const auto &resource = resources[i];
        response += std::get<0>(resource) + " " + std::to_string(std::get<1>(resource));
        if (i < resources.size() - 1) {
            response += ", ";
        }
    }
    response += "]\n";
    send_message(response);
}

void Player::set_down_resource(Server &server, std::vector<std::string> args)
{
    if (args.size() != 1) {
        send_message("ko\n");
        return;
    }

    Resource resource = parse_resource(args[0]);
    if (inventory.resources[idx(resource)] <= 0) {
        send_message("ko\n");
        return;
    }

    inventory.resources[idx(resource)]--;
    server._map[position[1]][position[0]].inventory.resources[idx(resource)]++;
    //notify the gui that a resource has been dropped on the tile
    auto self = std::dynamic_pointer_cast<Player>(server._clients[control_fd]);
    server._gui_subject.Notify([self, resource](Client* c) {
        static_cast<Gui*>(c)->pdr(self, idx(resource));
    });
    send_message("ok\n");
}

void Player::take_resource(Server &server, std::vector<std::string> args)
{
    if (args.size() != 1) {
        send_message("ko\n");
        return;
    }

    Resource resource = parse_resource(args[0]);
    if (server._map[position[1]][position[0]].inventory.resources[idx(resource)] <= 0) {
        send_message("ko\n");
        return;
    }

    inventory.resources[idx(resource)]++;
    server._map[position[1]][position[0]].inventory.resources[idx(resource)]--;
    //notify the gui that a resource has been taken from the tile
    auto self = std::dynamic_pointer_cast<Player>(server._clients[control_fd]);
    server._gui_subject.Notify([self, resource](Client* c) {
        static_cast<Gui*>(c)->pgt(self, idx(resource));
    });
    send_message("ok\n");
}

void Player::eject(Server &server)
{
    Tiles &current_tile = server._map[position[1]][position[0]];

    for (const auto &player : current_tile.players) {
        if (player->getId() != player_id) { //don't eject yourself
            switch (orientation) {
                case NORTH:
                    player->set_position(player->position[0], (player->position[1] - 1 + server.getMapHeight()) % server.getMapHeight());
                    break;
                case EAST:
                    player->set_position((player->position[0] + 1) % server.getMapWidth(), player->position[1]);
                    break;
                case SOUTH:
                    player->set_position(player->position[0], (player->position[1] + 1) % server.getMapHeight());
                    break;
                case WEST:
                    player->set_position((player->position[0] - 1 + server.getMapWidth()) % server.getMapWidth(), player->position[1]);
                    break;
            }
            //notify the ejected player with eject: K\n
            //where K is the direction of the tile where the pushed player is coming from. (so reverse of the orientation of the player that is doing the ejecting)
            player->send_message("eject" + std::to_string((orientation + 2) % 4) + "\n");
        }
    }
    auto self = std::dynamic_pointer_cast<Player>(server._clients[control_fd]);
    server._gui_subject.Notify([self](Client* c) {
        static_cast<Gui*>(c)->pex(self);
    });
    send_message("ok\n");
}

void Player::broadcast(Server &server, std::vector<std::string> args)
{
    if (args.empty() || args.size() != 1) {
        send_message("ko\n");
        return;
    }

    //he server will then send the following line to all of its clients (except the one that sent it):
    //message K, text\n
    //where K is the tile indicating the direction the sound is coming from.
    send_message("ok\n");
}

void Player::fork(Server &server)
{
    //TODO: implement egg logic first
    send_message("ok\n");
    //notify the gui that a new egg has been laid
    auto self = std::dynamic_pointer_cast<Player>(server._clients[control_fd]);
    server._gui_subject.Notify([self](Client* c) {
        static_cast<Gui*>(c)->pfk(self);
    });
}

void Player::connect_nbr(Server &server)
{
    //sends the number of available spots in the team of the player

    int slots_left = 0;
    for (const auto &team : server.teams) {
        if (team->name == team_name) {
            slots_left = team->spots_left;
            break;
        }
    }
    send_message("connect_nbr " + std::to_string(slots_left) + "\n");
}
