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

// game timing, expressed in ticks (the loop advances one tick every time_unit ms).
// one tick == 7/f s == 7 of the pdfs base tu, so a pdf cost of N base
// tu is N/7 ticks (the /7 holds for any f), costs the pdf gives in base units:
// food drain 126, incantation 300, resource respawn every 20
const long long FOOD_DRAIN_TICKS = 18;    // 126
const long long RESPAWN_TICKS = 20;        // 20

//ACTION DELAY
const long long INCANTATION_TICKS = 300;   // 300

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
