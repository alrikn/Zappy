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
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
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

    const std::unordered_map<std::string, std::function<void(const char *)>> handlers = {
        {"-p", [&](const char *v){ port    = atoi(v);  }},
        {"-x", [&](const char *v){ width   = atoi(v);  }},
        {"-y", [&](const char *v){ height  = atoi(v);  }},
        {"-n", [&](const char *v){ team_names.push_back(v); }},
        {"-c", [&](const char *v){ clients = atoi(v);  }},
        {"-f", [&](const char *v){ freq    = atoll(v); }},
    };

    for (int i = 1; i < argc - 1; i++) {
        auto it = handlers.find(argv[i]);
        if (it != handlers.end())
            it->second(argv[++i]);
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
    Server server = handle_arguments(argc, argv);
    try {
        server.run();
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 84;
    }
    return 0;
}