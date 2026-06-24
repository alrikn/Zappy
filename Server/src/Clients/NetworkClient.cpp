/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** NetworkClient
*/

#include "NetworkClient.hpp"
#include <unistd.h>
#include <cstring>

void NetworkClient::send(const std::string &msg)
{
    write(fd, msg.c_str(), msg.length());
}

std::string NetworkClient::receive()
{
    char buf[1024];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);

    if (n < 0) {
        perror("read");
        return "";
    }
    buf[n] = '\0';
    std::string result(buf);
    // strip \r so callers always see \n line endings
    std::string::size_type pos = 0;
    while ((pos = result.find('\r', pos)) != std::string::npos)
        result.erase(pos, 1);
    return result;
}
