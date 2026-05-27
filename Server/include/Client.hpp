/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** Client
*/

#ifndef INCLUDED_CLIENT_HPP
    #define INCLUDED_CLIENT_HPP


#include <string>
class Client
{
    private:
    protected:
    public:
        Client();
        ~Client() = default;


        /*todo make it do communication between server and client*/
        /*this will be the parent class of both the gui and the player (ai)*/ //(unless decided otherwise)

        int fd;


        /* client functions*/
        void send_message(std::string message);
        std::string receive_message();

};


#endif
