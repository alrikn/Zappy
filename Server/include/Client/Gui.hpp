/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** Gui
*/

#ifndef INCLUDED_GUI_HPP
    #define INCLUDED_GUI_HPP

#include "Client.hpp"
#include "Struct.hpp"
#include <iostream>
#include <memory>
#include <vector>


class Player;
class Gui : public Client
{
    private:
    protected:
    public:
        Gui(Subject &subject, int control_fd = -1) : Client(GUI, subject, control_fd) {};
        ~Gui() = default;

        /*
        gui a bit of a weird one, beacause it needs to be able to request data from the server, and also receive data from the server.

        I'm thinking of implementing a behavioral observer pattern,
        where the gui can subscribe to certain events on the server,
        and the server will notify the gui when those events happen.

        */

        void parse_command(const std::string command, Server &server) override;

        /*these are all the commands that the server can receive from the gui*/

        void msz(Server &server); //map size
        void bct(Server &server, std::vector<std::string> args); //content of a tile
        void mct(Server &server); //content of the map (all tiles)
        void tna(Server &server); //team names
        void ppo(Server &server, std::vector<std::string> args); //player position
        void plv(Server &server, std::vector<std::string> args); //player level
        void pin(Server &server, std::vector<std::string> args); //player inventory
        void sgt(Server &server); //server time unit (set gui time unit to the server time unit)
        void sst(Server &server, std::vector<std::string> args); //set server time unit (set server time unit to the gui time unit)

        /*these are all things that the server sends without explicit request to the gui*/
        //TODO: figure out what arguments these functions need to take
        void pnw(std::shared_ptr<Player> player); //new player
        void pex(std::shared_ptr<Player> player); //player expulsion
        void pbc(std::shared_ptr<Player> player, std::string message); //player broadcast
        void pic(int incantaion_level, std::vector<std::shared_ptr<Player>> players); //player incantation start
        void pie(int x, int y, bool result); //player incantation end
        void pfk(Server &server, std::shared_ptr<Player> player); //player laying egg (start action)
        void pdr(Server &server, std::shared_ptr<Player> player); //player drop
        void pgt(Server &server); //player take
        void pdi(Server &server); //player death
        void enw(Server &server); //egg laid (end action)
        void seg(Server &server); //end of game
        void smg(Server &server, std::string message); //server message



};


#endif
