/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** Struct
*/

#ifndef INCLUDED_STRUCT_HPP
    #define INCLUDED_STRUCT_HPP


//on a 10 by 10 world there would be 50 food and 5 thystane
const float FOOD_DENSITY = 0.5;
const float LINEMATE_DENSITY = 0.3;
const float DERAUMERE_DENSITY = 0.15;
const float SIBUR_DENSITY = 0.1;
const float MENDIANE_DENSITY = 0.1;
const float PHIRAS_DENSITY = 0.08;
const float THYSTAME_DENSITY = 0.05;

typedef struct resources
{
    int food = 0; //food also represents lifespan of the player, if it reaches 0 the player dies
    int linemate = 0;
    int deraumere = 0;
    int sibur = 0;
    int mendiane = 0;
    int phiras = 0;
    int thystame = 0;
} resources_t;


#endif
