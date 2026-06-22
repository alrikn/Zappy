/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** main
*/

#include "Server.hpp"
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

volatile sig_atomic_t g_shutdown_requested = 0;

void signal_handler(int signal)
{
    if (signal == SIGINT) {
        g_shutdown_requested = 1;
    }
}

static void usage(const char *prog)
{
    std::cerr << "USAGE: " << prog
              << " -p port -x width -y height -n name [-n name ...] -c clientsNb -f freq\n";
}

Server handle_arguments(int argc, char **argv)
{
    int port = -1, width = -1, height = -1, clients = -1;
    long long freq = -1;
    std::vector<std::string> team_names;

    for (int i = 1; i < argc; i++) {
        std::string flag = argv[i];
        // -n takes one or more names  
        // up to the next flag
        if (flag == "-n") {
            while (i + 1 < argc && argv[i + 1][0] != '-')
                team_names.push_back(argv[++i]);
            continue;
        }
        // every otger flag needs 1 value, if there are extra return error
        if (i + 1 >= argc) {
            usage(argv[0]);
            exit(84);
        }
        if (flag == "-p")      port    = atoi(argv[++i]);
        else if (flag == "-x") width   = atoi(argv[++i]);
        else if (flag == "-y") height  = atoi(argv[++i]);
        else if (flag == "-c") clients = atoi(argv[++i]);
        else if (flag == "-f") freq    = atoll(argv[++i]);
        else { // unknown flag/garbage token
            usage(argv[0]);
            exit(84);
        }
    }

    if (port < 1 || width < 1 || height < 1 || clients < 1 || freq < 1 || team_names.empty()) {
        usage(argv[0]);
        exit(84);
    }

    return Server(port, width, height, team_names, clients, freq);
}

int main(int argc, char **argv)
{
    std::signal(SIGINT, signal_handler);
    try {
        Server server = handle_arguments(argc, argv);
        server.run();
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 84;
    }
    return 0;
}