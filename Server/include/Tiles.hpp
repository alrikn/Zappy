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
#include "Struct.hpp"
#include <memory>
class Tiles
{
    private:
    protected:
    public:
        Tiles() = default; //by default, tiles are empty
        ~Tiles() = default;

        resources_t resources; //the resources on the tile, it contains the number of each resource on the tile
        std::shared_ptr<Client> client = nullptr; //the client on the tile, if there is one (can be null)
        std::shared_ptr<Egg> egg = nullptr; //the egg on the tile, if there is one (can be null)

};


#endif
