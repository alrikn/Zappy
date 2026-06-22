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
#include <functional>
#include <list>
#include <memory>

class Subject : public ISubject
{
    private:
        std::list<std::weak_ptr<Client>> _observers;
        std::string _message = "";
    protected:
    public:
        Subject() = default;
        ~Subject() = default;

        void Attach(std::shared_ptr<Client> observer) override {
            _observers.push_back(observer); //implicit conversion
        }

        //note: we are comparing the raw pointer of the observer to remove it, this is because the observer might be expired and we dont want to compare expired weak_ptrs
        void Detach(Client *observer) override {
            _observers.remove_if([observer](const std::weak_ptr<Client>& w) {
                auto s = w.lock(); //lock works by creating a shared_ptr from the weak_ptr, if the weak_ptr is expired it returns an empty shared_ptr
                return (!s || s.get() == observer);
            });
        }

        void Notify() override {
            _observers.remove_if([](const std::weak_ptr<Client>& w) { return w.expired(); });
            for (auto& w : _observers) {
                if (auto obs = w.lock())
                    obs->Update(_message);
            }
        }

        void Notify(std::function<void(Client *)> fn) override {
            _observers.remove_if([](const std::weak_ptr<Client>& w) { return w.expired(); });
            for (auto& w : _observers) {
                if (auto obs = w.lock())
                    fn(obs.get());
            }
        }

        void CreateMessage(std::string message) {
            _message = message;
            Notify();
        };
};


#endif
