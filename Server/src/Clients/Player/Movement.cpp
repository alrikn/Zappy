/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** Movement
*/

#include "Player.hpp"
#include "Server.hpp"
#include "Struct.hpp"
#include <vector>


void Player::move_forward(Server &server)
{
    //we will use the server funtion to move the player. i made it for this express purpose

    int new_x = position[0];
    int new_y = position[1];

    switch (orientation) {
        case NORTH:
            new_y = (position[1] - 1 + server.getMapHeight()) % server.getMapHeight();
            break;
        case EAST:
            new_x = (position[0] + 1) % server.getMapWidth();
            break;
        case SOUTH:
            new_y = (position[1] + 1) % server.getMapHeight();
            break;
        case WEST:
            new_x = (position[0] - 1 + server.getMapWidth()) % server.getMapWidth();
            break;
    }
    server.move_player(*this, new_x, new_y);
    server.send_message_queue.add_message(server, control_fd, "ok\n", ClientCommandDelayMap.at(FORWARD));
}

void Player::turn_right(Server &server)
{
    orientation = static_cast<orientation_t>((orientation % 4) + 1);
    server.send_message_queue.add_message(server, control_fd, "ok\n", ClientCommandDelayMap.at(RIGHT));
}

void Player::turn_left(Server &server)
{
    orientation = static_cast<orientation_t>((orientation + 2) % 4 + 1);
    server.send_message_queue.add_message(server, control_fd, "ok\n", ClientCommandDelayMap.at(LEFT));
}


//debatable if this goes in the movement, but for now goodenough
void Player::look(Server &server)
{
    //how it works:
    // we respond with a array: player, object-on-tile1, ..., object-on-tileP,...]

    //if there are more than one thing on a tile, we separate them with a space.

    //we start with the tile the player is on
    //the 3 in front of him (left to right)
    //and then the 5 tiles beyond that (left to right)
    //and then the 7 tiles beyond that (left to right)

    //so it looks like an updide down pyramid with a height of 4 (including the tile the player is on)

    //we will use the server map to get the info we need, and we will use the player position and orientation to know which tiles to look at


    //we do this by first calculating the coordinates of the tiles we need to look at, and then we get the info from the server map and format it in the way we need to send it to the client.

    std::vector<std::vector<int>> tiles_to_look_at;

    for (int i = 1; i < 5; i++) { //cus direction now starts at 1
        for (int j = -i; j <= i; j++) {
            int tile_x, tile_y;
            switch (orientation) {
                case NORTH:
                    tile_x = (position[0] + j + server.getMapWidth()) % server.getMapWidth();
                    tile_y = (position[1] - i + server.getMapHeight()) % server.getMapHeight();
                    break;
                case EAST:
                    tile_x = (position[0] + i) % server.getMapWidth();
                    tile_y = (position[1] + j + server.getMapHeight()) % server.getMapHeight();
                    break;
                case SOUTH:
                    tile_x = (position[0] + j + server.getMapWidth()) % server.getMapWidth();
                    tile_y = (position[1] + i) % server.getMapHeight();
                    break;
                case WEST:
                    tile_x = (position[0] - i + server.getMapWidth()) % server.getMapWidth();
                    tile_y = (position[1] + j + server.getMapHeight()) % server.getMapHeight();
                    break;
                default:
                    throw std::runtime_error("Invalid orientation");
            }
            tiles_to_look_at.push_back({tile_x, tile_y});
        }
    }


    std::vector<std::string> response_parts;

    for (const std::vector<int> &tile_coords : tiles_to_look_at) {
        int x = tile_coords[0];
        int y = tile_coords[1];
        std::string tile_info;

        //first we add the players on the tile
        for (auto player : server._map[y][x].players) {
            tile_info += "player ";
        }
        //then we add the resources on the tile
        for (const std::string &resource : give_resources_name(server._map[y][x].inventory)) {
            tile_info += resource + " ";
        }

        if (!tile_info.empty()) {
            tile_info.pop_back(); //remove the last space
        }

        response_parts.push_back(tile_info);
    }

    std::string final_response = "[";
    for (size_t i = 0; i < response_parts.size(); i++) {
        final_response += response_parts[i];
        if (i != response_parts.size() - 1) {
            final_response += ", ";
        }
    }
    final_response += "]\n";
    server.send_message_queue.add_message(server, control_fd, final_response, ClientCommandDelayMap.at(LOOK));
}
