/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** Movement
*/

#include "Player.hpp"
#include "Server.hpp"


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
    send_message("ok\n");
}

void Player::turn_right()
{
    orientation = static_cast<orientation_t>((orientation + 1) % 4);
    send_message("ok\n");
}

void Player::turn_left()
{
    orientation = static_cast<orientation_t>((orientation + 3) % 4);
    send_message("ok\n");
}

//debatable if this goes in the movement, but for now goodenough
void Player::look(Server &server)
{
    //TODO
    send_message("ok\n");
}