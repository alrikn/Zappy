/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** Client
*/

#include "Client.hpp"
#include "Subject.hpp"

void Client::Update(std::string message)
{
    send_message(message);
}

void Client::RemoveMeFromList()
{
    _subject.Detach(this);
}
