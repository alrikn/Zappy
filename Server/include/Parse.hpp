/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** Parse
*/

#ifndef INCLUDED_PARSE_HPP
    #define INCLUDED_PARSE_HPP

#include <unordered_map>
#include <string>

//Commands that the player can send to the server
enum PlayerCommands {
    FORWARD,
    RIGHT,
    LEFT,
    LOOK,
    INVENTORY,
    BROADCAST,
    CONNECT_NBR,
    FORK,
    EJECT,
    TAKE,
    SET,
    INCANTATION
};

const std::unordered_map<std::string, PlayerCommands> ClientCommandMap = {
    {"Forward",     FORWARD},
    {"Right",       RIGHT},
    {"Left",        LEFT},
    {"Look",        LOOK},
    {"Inventory",   INVENTORY},
    {"Broadcast",   BROADCAST},
    {"Connect_nbr", CONNECT_NBR},
    {"Fork",        FORK},
    {"Eject",       EJECT},
    {"Take",        TAKE},
    {"Set",         SET},
    {"Incantation", INCANTATION},
};

//Commands that the GUI can send to the server
enum GuiCommands {
    MSZ, //map size
    BCT, //content of a cell
    MCT, //content of the map (all cells)
    TNA, //team names
    PPO, //player position
    PLV, //player level
    PIN, //player inventory
    SGT, //server time
};

const std::unordered_map<std::string, GuiCommands> GuiCommandMap = {
    {"msz", MSZ},
    {"bct", BCT},
    {"mct", MCT},
    {"tna", TNA},
    {"ppo", PPO},
    {"plv", PLV},
    {"pin", PIN},
    {"sgt", SGT},
};

#endif
