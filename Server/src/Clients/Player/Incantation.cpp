/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** Incantation
*/

#include "Player.hpp"
#include "Server.hpp"
#include <memory>
#include <string>
#include <sys/socket.h>
#include "Server.hpp"
#include <vector>

/*
** ELEVATION REQUIREMENTS
** 1->2 : 1 player, 1 linemate
** 2->3 : 2 players, 1 linemate, 1 deraumere, 1 sibur
** 3->4 : 2 players, 2 linemate, 1 sibur, 2 phiras
** 4->5 : 4 players, 1 linemate, 1 deraumere, 2 sibur, 1 phiras
** 5->6 : 4 players, 1 linemate, 2 deraumere, 1 sibur, 3 mendiane
** 6->7 : 6 players, 1 linemate, 2 deraumere, 3 sibur, 1 phiras
** 7->8 : 6 players, 2 linemate, 2 deraumere, 2 sibur, 2 mendiane, 2 phiras, 1 thystame
*/
static bool check_requirements(int level, int players, const Inventory &inv)
{
    switch (level) {
        case 1:
            return players >= 1
                && inv.resources[idx(Resource::Linemate)] >= 1;
        case 2:
            return players >= 2
                && inv.resources[idx(Resource::Linemate)] >= 1
                && inv.resources[idx(Resource::Deraumere)] >= 1
                && inv.resources[idx(Resource::Sibur)] >= 1;
        case 3:
            return players >= 2
                && inv.resources[idx(Resource::Linemate)] >= 2
                && inv.resources[idx(Resource::Sibur)] >= 1
                && inv.resources[idx(Resource::Phiras)] >= 2;
        case 4:
            return players >= 4
                && inv.resources[idx(Resource::Linemate)] >= 1
                && inv.resources[idx(Resource::Deraumere)] >= 1
                && inv.resources[idx(Resource::Sibur)] >= 2
                && inv.resources[idx(Resource::Phiras)] >= 1;
        case 5:
            return players >= 4
                && inv.resources[idx(Resource::Linemate)] >= 1
                && inv.resources[idx(Resource::Deraumere)] >= 2
                && inv.resources[idx(Resource::Sibur)] >= 1
                && inv.resources[idx(Resource::Mendiane)] >= 3;
        case 6:
            return players >= 6
                && inv.resources[idx(Resource::Linemate)] >= 1
                && inv.resources[idx(Resource::Deraumere)] >= 2
                && inv.resources[idx(Resource::Sibur)] >= 3
                && inv.resources[idx(Resource::Phiras)] >= 1;
        case 7:
            return players >= 6
                && inv.resources[idx(Resource::Linemate)] >= 2
                && inv.resources[idx(Resource::Deraumere)] >= 2
                && inv.resources[idx(Resource::Sibur)] >= 2
                && inv.resources[idx(Resource::Mendiane)] >= 2
                && inv.resources[idx(Resource::Phiras)] >= 2
                && inv.resources[idx(Resource::Thystame)] >= 1;
        default:
            return false;
    }
}

static void consume_resources(Inventory &inv, int level)
{
    switch (level) {
        case 1:
            inv.resources[idx(Resource::Linemate)] -= 1;
            break;
        case 2:
            inv.resources[idx(Resource::Linemate)] -= 1;
            inv.resources[idx(Resource::Deraumere)] -= 1;
            inv.resources[idx(Resource::Sibur)] -= 1;
            break;
        case 3:
            inv.resources[idx(Resource::Linemate)] -= 2;
            inv.resources[idx(Resource::Sibur)] -= 1;
            inv.resources[idx(Resource::Phiras)] -= 2;
            break;
        case 4:
            inv.resources[idx(Resource::Linemate)] -= 1;
            inv.resources[idx(Resource::Deraumere)] -= 1;
            inv.resources[idx(Resource::Sibur)] -= 2;
            inv.resources[idx(Resource::Phiras)] -= 1;
            break;
        case 5:
            inv.resources[idx(Resource::Linemate)] -= 1;
            inv.resources[idx(Resource::Deraumere)] -= 2;
            inv.resources[idx(Resource::Sibur)] -= 1;
            inv.resources[idx(Resource::Mendiane)] -= 3;
            break;
        case 6:
            inv.resources[idx(Resource::Linemate)] -= 1;
            inv.resources[idx(Resource::Deraumere)] -= 2;
            inv.resources[idx(Resource::Sibur)] -= 3;
            inv.resources[idx(Resource::Phiras)] -= 1;
            break;
        case 7:
            inv.resources[idx(Resource::Linemate)] -= 2;
            inv.resources[idx(Resource::Deraumere)] -= 2;
            inv.resources[idx(Resource::Sibur)] -= 2;
            inv.resources[idx(Resource::Mendiane)] -= 2;
            inv.resources[idx(Resource::Phiras)] -= 2;
            inv.resources[idx(Resource::Thystame)] -= 1;
            break;
        default:
            break;
    }
}

// phase 1: called immediately when incnataton is dequeued in advance_game
// checks requirements, freezes all same level players on the tile for INCANTATION_TICKS,
// sends "Elevation underway" to every frozen participant
// returns false and sends ko if requirements are not met
bool Player::incantation_start(Server &server)
{
    auto &tile = server._map[position[1]][position[0]];

    int players_at_level = 0;
    for (const auto &p : tile.players)
        if (p->level == level)
            players_at_level++;

    if (!check_requirements(level, players_at_level, tile.inventory)) {
        command_failed(server, INCANTATION);
        return false;
    }

    //tell the gui that incant is underway
    auto self = std::dynamic_pointer_cast<Player>(server._clients[control_fd]);
    server._gui_subject.Notify([self, &server](Client* c) {
        static_cast<Gui*>(c)->pic(self->level, server._map[self->position[1]][self->position[0]].players);
    });

    long long deadline = server.tick + INCANTATION_TICKS; // long long to be safe idk maybe overkill

    for (const auto &p : tile.players) {
        if (p->level == level) {
            p->in_incantation = true;
            p->busy = true;
            p->running_cmd = {INCANTATION, {}};
            p->action_done_at = deadline;
            server.send_message_queue.add_message(server, p->control_fd, "Elevation underway\n");
        }
    }
    return true;
}

// phase  2: called by execute_command when action_done_at fires
// re checks requirements (subject: verified at start and end)
// if the check passes: consumes stones, levels up all participants, unfreezes
// early returns if in_incantation is already false, means this participant was
// already processed by the first participant's finish call in the same loop iteration
void Player::incantation(Server &server)
{
    if (!in_incantation)
        return;

    auto &tile = server._map[position[1]][position[0]];

    //TODO: put incantation start here

    // gather participants still alive on tile
    std::vector<std::shared_ptr<Player>> participants;
    for (const auto &p : tile.players)
        if (p->in_incantation && p->level == level)
            participants.push_back(p);

    if (!check_requirements(level, static_cast<int>(participants.size()), tile.inventory)) {
        for (const auto &p : participants) {
            command_failed(server, INCANTATION);
            p->in_incantation = false;
        }
        return;
    }

    int old_level = level;
    int new_level = old_level + 1;

    consume_resources(tile.inventory, old_level);

    std::string response = "Current level: " + std::to_string(new_level) + "\n";
    for (const auto &p : participants) {
        p->level = new_level;
        p->in_incantation = false;
        server.send_message_queue.add_message(server, p->control_fd, response, ClientCommandDelayMap.at(INCANTATION));
    }

    //notify the gui that the incantation has finished
    auto self = std::dynamic_pointer_cast<Player>(server._clients[control_fd]);
    server._gui_subject.Notify([self](Client* c) {
        static_cast<Gui*>(c)->pie(self->position[0], self->position[1], true);
    });

    // win con: any team with >= 6 players at level 8
    if (new_level == 8) {
        for (const auto &team : server.teams) {
            int count = 0;
            for (const auto &[fd, c] : server._clients) {
                if (c->get_type() != PLAYER) continue;
                auto pp = std::dynamic_pointer_cast<Player>(c);
                if (pp && pp->team_name == team->name && pp->level == 8)
                    count++;
            }
            if (count >= 6) {
                server.running = false;
                return;
            }
        }
    }
}
