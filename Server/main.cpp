/*
** EPITECH PROJECT, 2026
** Zappy
** File description:
** main
*/

#include "Server.hpp"
#include <csignal>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
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
    << " -p port: port number" << std::endl
    << " -x width: map width" << std::endl
    << " -y height: map height" << std::endl
    << " -n name1 name2 ...: team names" << std::endl
    << " -c clients_per_team: number of initial clients per team" << std::endl
    << " -f freq: time unit in ms" << std::endl
    << "Example: " << prog << " -p 4242 -x 10 -y 10 -n steve -c 7 -f 100"
    << std::endl;
}

static std::vector<std::string> collect_team_names(int &i, int argc, char **argv)
{
    std::vector<std::string> names;
    while (i + 1 < argc && argv[i + 1][0] != '-')
        names.push_back(argv[++i]);
    return names;
}

static void validate_server_args(const ServerArgs &args, const char *prog)
{
    if (args.port < 1 || args.width < 1 || args.height < 1
        || args.clients < 1 || args.freq < 1 || args.team_names.empty()) {
        usage(prog);
        exit(84);
    }

    std::unordered_set<std::string> seen;
    for (const auto &name : args.team_names) {
        if (name.empty()) {
            std::cerr << "Error: team name cannot be empty." << std::endl;
            exit(84);
        }
        if (name == "GRAPHIC") {
            std::cerr << "Error: 'GRAPHIC' is a reserved team name." << std::endl;
            exit(84);
        }
        if (!seen.insert(name).second) {
            std::cerr << "Error: duplicate team name '" << name << "'." << std::endl;
            exit(84);
        }
    }
}

static ServerArgs parse_server_args(int argc, char **argv)
{
    using Setter = std::function<void(ServerArgs &, const std::string &)>;
    static const std::unordered_map<std::string, Setter> handlers = {
        {"-p", [](ServerArgs &a, const std::string &v) { a.port    = std::stoi(v); }},
        {"-x", [](ServerArgs &a, const std::string &v) { a.width   = std::stoi(v); }},
        {"-y", [](ServerArgs &a, const std::string &v) { a.height  = std::stoi(v); }},
        {"-c", [](ServerArgs &a, const std::string &v) { a.clients = std::stoi(v); }},
        {"-f", [](ServerArgs &a, const std::string &v) { a.freq    = std::stoll(v); }},
    };

    ServerArgs args;
    for (int i = 1; i < argc; i++) {
        std::string flag = argv[i];

        if (flag == "-n") {
            args.team_names = collect_team_names(i, argc, argv);
            continue;
        }

        auto it = handlers.find(flag);
        if (it == handlers.end() || i + 1 >= argc) {
            usage(argv[0]);
            exit(84);
        }
        it->second(args, argv[++i]);
    }
    return args;
}

Server handle_arguments(int argc, char **argv)
{
    ServerArgs args;
    try {
        args = parse_server_args(argc, argv);
    } catch (const std::exception &) {
        usage(argv[0]);
        exit(84);
    }
    validate_server_args(args, argv[0]);
    return Server(args.port, args.width, args.height, args.team_names, args.clients, args.freq);
}

int main(int argc, char **argv)
{
    std::signal(SIGINT, signal_handler);
    std::signal(SIGPIPE, signal_handler);
    try {
        Server server = handle_arguments(argc, argv);
        server.run();
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 84;
    }
    return 0;
}
