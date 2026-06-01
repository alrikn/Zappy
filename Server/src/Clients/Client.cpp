/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** Client
*/

#include "Client.hpp"
#include <unistd.h>


void Client::send_message(std::string message)
{
    write(control_fd, message.c_str(), message.length());
}

std::string Client::receive_message()
{
    char buffer[1024];
    ssize_t bytes_read = read(control_fd, buffer, sizeof(buffer) - 1);

    if (bytes_read < 0) {
        perror("read");
        return "";
    }
    buffer[bytes_read] = '\0'; // Null-terminate the string
    return std::string(buffer);
}

void Client::Update(std::string message)
{
    send_message(message);
}
