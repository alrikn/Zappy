/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** Queue
*/

#include "SendMessageQueue.hpp"
#include "Server.hpp"
#include <unistd.h>

void SendMessageQueue::add_message(Server &server, int client_fd,
    const std::string& message, int delay)
{
    Message msg;

    msg.client_fd = client_fd;
    msg.message = message;
    msg.tick = server.tick + delay;
    _messages.push_back(msg);
}

void SendMessageQueue::send_messages(long long current_tick)
{
    size_t write_size = 0;
    for (auto it = _messages.begin(); it != _messages.end();) {
        if (it->tick <= current_tick) {
            write_size = write(it->client_fd, it->message.c_str(), it->message.length());
            if (write_size < it->message.length()) {
                perror("write");
            }
            it = _messages.erase(it);
        } else {
            it++;
        }
    }
}

void SendMessageQueue::clear_messages_for_client(int client_fd)
{
    for (auto it = _messages.begin(); it != _messages.end();) {
        if (it->client_fd == client_fd) {
            it = _messages.erase(it);
        } else {
            it++;
        }
    }
}

void SendMessageQueue::clear_all_messages()
{
    _messages.clear();
}
