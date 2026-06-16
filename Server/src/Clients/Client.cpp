/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** Client
*/

#include "Client.hpp"
#include "Subject.hpp"
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
    //now we will remove any \r in the buffer, because the gui sends \r\n and we only want \n
    std::string new_buffer(buffer);
    std::string::size_type pos = 0;
    while ((pos = new_buffer.find('\r', pos)) != std::string::npos) {
        new_buffer.erase(pos, 1);
    }
    return new_buffer;
}

void Client::Update(std::string message)
{
    send_message(message);
}

void Client::RemoveMeFromList()
{
    _subject.Detach(this);
}
