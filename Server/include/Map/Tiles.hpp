/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** Tiles
*/

#ifndef INCLUDED_TILES_HPP
    #define INCLUDED_TILES_HPP


#include "Client.hpp"
#include "Egg.hpp"
#include "Player.hpp"
#include "Struct.hpp"
#include <vector>
#include <memory>

class Tiles
{
    private:
    protected:
    public:
        Tiles() = default; //by default, tiles are empty
        ~Tiles() = default;

        Inventory inventory; //the inventory of the tile, it contains the number of each resource on the tile
        std::vector<std::shared_ptr<Player>> players; //the clients on the tile, if there are any (can be empty)
        std::shared_ptr<Egg> egg = nullptr; //the egg on the tile, if there is one (can be null)

        bool remove_specific_client(int player_num); //returns true or false depending on whether the client was found and removed or not

};


#endif
