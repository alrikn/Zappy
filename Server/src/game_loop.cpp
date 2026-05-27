/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** main_loop
*/

#include "Server.hpp"

#include <csignal>
#include <cstdio>
#include <cstring>
#include <memory>
#include <chrono>

extern volatile sig_atomic_t g_shutdown_requested;

void Server::game_tick()
{
    tick++;
}

void Server::run()
{
    auto next_tick = std::chrono::steady_clock::now();

    while (!g_shutdown_requested && running) {
        auto now = std::chrono::steady_clock::now();
        if (now >= next_tick) {
            game_tick();
            std::cout << "tick: " << tick << std::endl;
            next_tick += std::chrono::milliseconds(time_unit);
        }
        now = std::chrono::steady_clock::now();
        int timeout = std::chrono::duration_cast<std::chrono::milliseconds>(next_tick - now).count();
        if (timeout < 0)
            timeout = 0;
        poll_clients(timeout);
    }
}