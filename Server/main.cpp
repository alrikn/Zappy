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

Server handle_arguments(int argc, char **argv)
{
    // TODO: Implement argument handling
    return Server(4242, 10, 10, {"Team1", "Team2"}, 5);
}

int main(int argc, char **argv)
{
    std::signal(SIGINT, signal_handler);
    Server server = handle_arguments(argc, argv);
    server.run();
    return 0;
}