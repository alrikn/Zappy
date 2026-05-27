/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** Client
*/

#ifndef INCLUDED_CLIENT_HPP
    #define INCLUDED_CLIENT_HPP


#include <string>

typedef enum client_type {
    GUI,
    PLAYER
} client_type_t;
class Client
{
    private:
    protected:
    public:
        Client(int control_fd = -1, client_type_t type = PLAYER) : control_fd(control_fd), type(type) {};
        ~Client() = default;


        /*todo make it do communication between server and client*/
        /*this will be the parent class of both the gui and the player (ai)*/ //(unless decided otherwise)

        int control_fd;
        std::string ctrl_buffer;
        client_type_t type;



        /* client functions*/
        void send_message(std::string message);
        std::string receive_message();

};


#endif
