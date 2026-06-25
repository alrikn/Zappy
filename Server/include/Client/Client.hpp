/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** Client
*/

#ifndef INCLUDED_CLIENT_HPP
    #define INCLUDED_CLIENT_HPP

#include "NetworkClient.hpp"
#include <string>

class Subject;
class Server;

typedef enum client_type {
    GUI,
    PLAYER
} client_type_t;

class Client
{
    private:
        Subject &_subject;

    protected:
        NetworkClient _net;

    public:
        client_type_t type;

        Client(client_type_t type, Subject &subject, int fd = -1)
            : _subject(subject), _net(fd), type(type) {}
        virtual ~Client() = default;

        int get_fd() const { return _net.fd; }
        bool write_to_buffer(const char *data, size_t n) { return _net.write_to_buffer(data, n); }
        bool read_line(std::string &out) { return _net.read_line(out); }

        void send_message(const std::string &message) { _net.send(message); }
        std::string receive_message() { return _net.receive(); }

        client_type_t get_type() const { return type; }

        virtual void Update(std::string message);
        void RemoveMeFromList();

        virtual void parse_command(const std::string command, Server &server) = 0;
};

#endif
