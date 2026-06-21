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

        void gui_start(Server &server); //this is called when the gui connects to the server, it will send all the data that the gui needs to know about the current state of the game
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
        void pie(int x, int y, bool succeeded); //player incantation end
        void pfk(std::shared_ptr<Player> player); //player laying egg (start action)
        void pdr(std::shared_ptr<Player> player, int resource_type); //player drop
        void pgt(std::shared_ptr<Player> player, int resource_type); //player take
        void pdi(std::shared_ptr<Player> player); //player death
        void ppo(std::shared_ptr<Player> player); //passive position update (after movement)
        void plv(std::shared_ptr<Player> player); //passive level update (after level-up or gui_start)
        void enw(int egg_id, int player_id, int x, int y); //egg laid (end action)
        void ebo(int egg_id); //egg hatching (start action)
        void edi(int egg_id); //egg death
        void seg(Server &server); //end of game
        void smg(std::string message); //server message



};


#endif
