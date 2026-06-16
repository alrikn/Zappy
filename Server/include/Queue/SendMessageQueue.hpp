/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** SendMessageQueue
*/

#ifndef INCLUDED_SENDMESSAGEQUEUE_HPP
    #define INCLUDED_SENDMESSAGEQUEUE_HPP

    #include "Server.hpp"
#include <string>
    #include <vector>
    #include <map>

struct Message {
    int client_fd;
    std::string message;
    long long tick = 0;
};

class Server;

class SendMessageQueue
{
    private:
        std::vector<Message> _messages;

    protected:
    public:
        SendMessageQueue() = default;
        ~SendMessageQueue() = default;

        /*add message to queue*/
        //the message as well as the tick at which it should be sent
        void add_message(Server &server, int client_fd, const std::string& message, int delay = 0);


        void send_messages(long long current_tick);
        void clear_messages_for_client(int client_fd);
        void clear_all_messages();

};


#endif
