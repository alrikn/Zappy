/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** NetworkClient
*/

#ifndef INCLUDED_NETWORKCLIENT_HPP
    #define INCLUDED_NETWORKCLIENT_HPP

#include <string>

class NetworkClient
{
    public:
        int fd;
        std::string buffer;

        NetworkClient(int fd = -1) : fd(fd) {}

        void send(const std::string &msg);
        std::string receive();
};

#endif
