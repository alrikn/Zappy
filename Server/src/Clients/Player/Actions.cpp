/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** Actions
*/

#include "Parse.hpp"
#include "Player.hpp"
#include "Server.hpp"
#include "Gui.hpp"
#include "Egg.hpp"
#include <memory>
#include <string>
#include <vector>




void Player::inventory_handle(Server &server)
{
    // protocol format: "[ food N, linemate N, ... thystame N ]\n"
    // space after [ and before ] so the AI parser's [1:]/[:-1] strips the spaces,
    // not letters/digits. all 7 resources always listed (zero counts included)
    static const char *names[] = {
        "food", "linemate", "deraumere", "sibur", "mendiane", "phiras", "thystame"
    };
    std::string response = "[";
    for (size_t i = 0; i < static_cast<size_t>(Resource::Count); i++) {
        response += " ";
        response += names[i];
        response += " ";
        response += std::to_string(inventory.resources[i]);
        if (i < static_cast<size_t>(Resource::Count) - 1)
            response += ",";
    }
    response += " ]\n";
    server.send_message_queue.add_message(server, control_fd, response);
}

void Player::set_down_resource(Server &server, std::vector<std::string> args)
{
    if (args.size() != 1) {
        command_failed(server, SET);
        return;
    }

    Resource resource = parse_resource(args[0]);
    if (inventory.resources[idx(resource)] <= 0) {
        command_failed(server, SET);
        return;
    }

    inventory.resources[idx(resource)]--;
    auto &tile = server._map[position[1]][position[0]];
    tile.inventory.resources[idx(resource)]++;
    //notify the gui that a resource has been dropped on the tile
    auto self = std::dynamic_pointer_cast<Player>(server._clients[control_fd]);
    if (!self) {
        command_failed(server, SET);
        return;
        return;
    }
    server._gui_subject.Notify([self, resource, &server](Client* c) {
        auto gui = static_cast<Gui*>(c);
        gui->pdr(self, idx(resource));
        gui->pin(server, {std::to_string(self->getId())});
        gui->bct(server, {std::to_string(self->getX()), std::to_string(self->getY())});
    });
    server.send_message_queue.add_message(server, control_fd, "ok\n", ClientCommandDelayMap.at(SET));
}

void Player::take_resource(Server &server, std::vector<std::string> args)
{
    if (args.size() != 1) {
        command_failed(server, TAKE);
        return;
    }

    Resource resource = parse_resource(args[0]);
    auto &tile = server._map[position[1]][position[0]];
    if (tile.inventory.resources[idx(resource)] <= 0) {
        command_failed(server, TAKE);
        return;
    }

    inventory.resources[idx(resource)]++;
    tile.inventory.resources[idx(resource)]--;
    //notify the gui that a resource has been taken from the tile
    auto self = std::dynamic_pointer_cast<Player>(server._clients[control_fd]);
    if (!self) {
        command_failed(server, TAKE);
        return;
    }
    server._gui_subject.Notify([self, resource, &server](Client* c) {
        auto gui = static_cast<Gui*>(c);
        gui->pgt(self, idx(resource));
        gui->pin(server, {std::to_string(self->getId())});
        gui->bct(server, {std::to_string(self->getX()), std::to_string(self->getY())});
    });
    server.send_message_queue.add_message(server, control_fd, "ok\n", ClientCommandDelayMap.at(TAKE));
}

void Player::eject(Server &server)
{
    // snapshot: move_player modifies the tiles players vector mid iteration
    std::vector<std::shared_ptr<Player>> to_eject;
    for (const auto &p : server._map[position[1]][position[0]].players)
        if (p->getId() != player_id)
            to_eject.push_back(p);

    // K is the diction from whic h the ejected player was pushed (opposite of ejectors facing)
    int k = (static_cast<int>(orientation) + 2) % 4 + 1;

    for (const auto &p : to_eject) {
        int nx = p->position[0], ny = p->position[1];
        switch (orientation) {
            case NORTH: ny = (ny - 1 + server.getMapHeight()) % server.getMapHeight(); break;
            case EAST:  nx = (nx + 1) % server.getMapWidth();  break;
            case SOUTH: ny = (ny + 1) % server.getMapHeight(); break;
            case WEST:  nx = (nx - 1 + server.getMapWidth()) % server.getMapWidth();  break;
        }
        server.move_player(*p, nx, ny);
        p->send_message("eject " + std::to_string(k) + "\n");
    }
    auto self = std::dynamic_pointer_cast<Player>(server._clients[control_fd]);
    if (!self) {
        command_failed(server, EJECT);
        return;
    }
    server._gui_subject.Notify([self](Client* c) {
        static_cast<Gui*>(c)->pex(self);
    });
    server.send_message_queue.add_message(server, control_fd, "ok\n", ClientCommandDelayMap.at(EJECT));
}

// K (0-8): torus direction from receiver to sender in receiver's local frame
// K=0 same tile, K=1 straight ahead, K=2 front-right ... K=8 front-left
static int broadcast_dir(int sx, int sy, int rx, int ry,
                          orientation_t orient, int W, int H)
{
    int dx = sx - rx;
    int dy = sy - ry;
    if (2 * dx >  W) dx -= W;
    if (2 * dx < -W) dx += W;
    if (2 * dy >  H) dy -= H;
    if (2 * dy < -H) dy += H;
    if (dx == 0 && dy == 0) return 0;
    // rotate world delta into receiver's local frame (+ly = forward, +lx = right)
    int lx, ly;
    switch (orient) {
        case NORTH: lx =  dx; ly = -dy; break;
        case EAST:  lx =  dy; ly =  dx; break;
        case SOUTH: lx = -dx; ly =  dy; break;
        case WEST:  lx = -dy; ly = -dx; break;
        default:    lx =  dx; ly = -dy; break;
    }
    if (lx == 0 && ly > 0) return 1;
    if (lx < 0  && ly > 0) return 2;
    if (lx < 0  && ly == 0) return 3;
    if (lx < 0  && ly < 0) return 4;
    if (lx == 0 && ly < 0) return 5;
    if (lx > 0  && ly < 0) return 6;
    if (lx > 0  && ly == 0) return 7;
    return 8; // lx > 0 && ly > 0
}

void Player::broadcast(Server &server, std::vector<std::string> args)
{
    if (args.empty()) {
        command_failed(server, BROADCAST);
        return;
    }
    std::string text;
    for (size_t i = 0; i < args.size(); i++) {
        if (i > 0) text += " ";
        text += args[i];
    }
    int W = server.getMapWidth();
    int H = server.getMapHeight();
    for (auto &[fd, client] : server._clients) {
        if (client->get_type() != PLAYER) continue;
        auto p = std::dynamic_pointer_cast<Player>(client);
        if (!p || p.get() == this) continue;
        int k = broadcast_dir(position[0], position[1],
                               p->position[0], p->position[1],
                               p->orientation, W, H);
        p->send_message("message " + std::to_string(k) + ", " + text + "\n");
    }
    //notify the gui that a broadcast has been sent
    auto self = std::dynamic_pointer_cast<Player>(server._clients[control_fd]);
    if (!self) {
        command_failed(server, BROADCAST);
        return;
    }
    server.send_message_queue.add_message(server, control_fd, "ok\n", ClientCommandDelayMap.at(BROADCAST));
    server._gui_subject.Notify([self, &args](Client* c) {
        static_cast<Gui*>(c)->pbc(self, args[0]);
    });
}

void Player::fork(Server &server)
{
    std::shared_ptr<Egg> new_egg;
    for (auto &team : server.teams) {
        if (team->name != team_name) continue;
        new_egg = std::make_shared<Egg>(team_name,
            std::vector<int>{position[0], position[1]});
        new_egg->parent_player_id = player_id;
        team->eggs.push_back(new_egg);
        team->spots_left++;
        break;
    }
    auto self = std::dynamic_pointer_cast<Player>(server._clients[control_fd]);
    if (!self) {
        command_failed(server, FORK);
        return;
    }
    if (new_egg) {
        int egg_id = new_egg->getId();
        int px = position[0];
        int py = position[1];
        int pid = player_id;
        server._gui_subject.Notify([self, egg_id, px, py, pid](Client* c) {
            auto gui = static_cast<Gui*>(c);
            gui->pfk(self);
            gui->enw(egg_id, pid, px, py);
        });
    } else {
        server._gui_subject.Notify([self](Client* c) {
            static_cast<Gui*>(c)->pfk(self);
        });
    }
    server.send_message_queue.add_message(server, control_fd, "ok\n", ClientCommandDelayMap.at(FORK));
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
    std::string response = std::to_string(slots_left) + "\n";
    server.send_message_queue.add_message(server, control_fd, response, ClientCommandDelayMap.at(CONNECT_NBR));
}
