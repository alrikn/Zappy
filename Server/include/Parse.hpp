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

//some player commands have a delay, so we need to define a delay map
const std::unordered_map<PlayerCommands, int> ClientCommandDelayMap = {
    {FORWARD, 7},
    {RIGHT, 7},
    {LEFT, 7},
    {LOOK, 7},
    {INVENTORY, 1},
    {BROADCAST, 7},
    {CONNECT_NBR, 0},
    {FORK, 42},
    {EJECT, 7},
    {TAKE, 7},
    {SET, 7},
    {INCANTATION, 300}
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
    SGT, //set gui time
    SST  //set server time
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
    {"sst", SST}
};

#endif
