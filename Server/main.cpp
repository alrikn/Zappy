/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** main
*/

#include "Server.hpp"
#include <csignal>

volatile sig_atomic_t g_shutdown_requested = 0;

void signal_handler(int signal)
{
    if (signal == SIGINT) {
        g_shutdown_requested = 1;
    }
}

int main(int argc, char **argv)
{
    std::signal(SIGINT, signal_handler);
    return 0;
}