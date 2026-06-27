/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** NetworkClient
*/

#ifndef INCLUDED_NETWORKCLIENT_HPP
    #define INCLUDED_NETWORKCLIENT_HPP

#include "CircularBuffer.hpp"
#include <string>

class NetworkClient
{
    public:
        int fd;

        NetworkClient(int fd = -1) : fd(fd) {}

        bool write_to_buffer(const char *data, size_t n) { return _cbuf.write(data, n); }
        bool read_line(std::string &out) { return _cbuf.read_line(out); }

        void send(const std::string &msg);
        std::string receive();

    private:
        CircularBuffer _cbuf;
};

#endif
