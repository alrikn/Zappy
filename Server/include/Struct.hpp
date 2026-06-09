/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** Struct
*/

#ifndef INCLUDED_STRUCT_HPP
    #define INCLUDED_STRUCT_HPP


//on a 10 by 10 world there would be 50 food and 5 thystane
#include <string>
#include <array>

const float FOOD_DENSITY = 0.5;
const float LINEMATE_DENSITY = 0.3;
const float DERAUMERE_DENSITY = 0.15;
const float SIBUR_DENSITY = 0.1;
const float MENDIANE_DENSITY = 0.1;
const float PHIRAS_DENSITY = 0.08;
const float THYSTAME_DENSITY = 0.05;

enum class Resource {
    Food,
    Linemate,
    Deraumere,
    Sibur,
    Mendiane,
    Phiras,
    Thystame,
    Count
};

Resource parse_resource(const std::string& name);

struct Inventory {
    std::array<int, static_cast<size_t>(Resource::Count)> resources{}; //Implicit instantiation of undefined template 'std::array<int, 7>'
};

constexpr size_t idx(Resource r) //so that i don't have to write static_cast<size_t>(Resource::Food) every time
{
    return static_cast<size_t>(r);
}

#endif
