/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** ISubject
*/

#ifndef INCLUDED_ISUBJECT_HPP
    #define INCLUDED_ISUBJECT_HPP

#include "Client.hpp"
#include <functional>
#include <memory>


class ISubject
{
    protected:
    public:
        virtual ~ISubject() = default;
        virtual void Attach(std::shared_ptr<Client> observer) = 0;
        virtual void Detach(Client *observer) = 0;
        virtual void Notify() = 0;
        virtual void Notify(std::function<void(Client *)> fn) = 0;
};

#endif
