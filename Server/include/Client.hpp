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

        /*helper funcs*/
        client_type_t get_type() const { return type; }


        /*observer behavioral pattern functions*/

        //this will inform the client of any updates from the server.
        // (essentially, when some kind of event happens to the server, we want to inform the puplic about it.)
        //this will probably mostly be used for the gui, but it could also be used for the player (for example, to inform the player of the result of a command they sent to the server)
        //we may need to change it up a bit and add some kind of enum to inform the client exactly what kind of command it is.
        virtual void update(std::string message) = 0;
};


#endif
