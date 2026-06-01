/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** Subject
*/

#ifndef INCLUDED_SUBJECT_HPP
    #define INCLUDED_SUBJECT_HPP

#include "Client.hpp"
#include "ISubject.hpp"
#include <iostream>
#include <list>

class Subject : public ISubject
{
    private:
        std::list<Client *> _observers;
        std::string _message = ""; //the message that will be sent to the observers when they are notified
    protected:
    public:

        void Attach(Client *observer) override {
            _observers.push_back(observer);
        }

        void Detach(Client *observer) override {
            _observers.remove(observer);
        }

        void Notify() override {
            for (auto observer : _observers) {
                observer->Update(_message);
            }
        }

        void CreateMessage(std::string message) {
            _message = message;
            Notify();
        };
};


#endif
