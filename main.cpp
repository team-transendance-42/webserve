// main.cpp
#include "includes/ServerConfig.hpp"
#include "includes/Server.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <csignal>

static volatile sig_atomic_t g_running = 1;

static void onSignal(int) { g_running = 0; }

int main(int argc, char *argv[])
{
    if (argc > 2) {
        std::cout << "Naughty, naughty: you can enter only one filename.conf"
                     " or leave empty for default.conf\n";
        return 0;
    }

    // Signal handling — graceful shutdown on Ctrl+C / SIGTERM
    struct sigaction sa{};
    sa.sa_handler = onSignal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    // Config file from argv or default
    std::string configFile = (argc == 2) ? argv[1] : "default.conf";
    std::cout << "webserv — loading " << configFile << "\n";

    try
    {
        // hardcoded config for testing, to be replaced by filename.conf parser
        ServerConfig cfg = createDefaultServerConfig();

        std::cout << "Config loaded:\n"
                  << "  host  = " << cfg.host << "\n"
                  << "  port  = " << cfg.port << "\n"
                  << "  names = ";
        for (const auto &n : cfg.server_names)
            std::cout << n << " ";
        std::cout << "\n  locations = " << cfg.locations.size() << "\n\n";

        Server server(cfg);
        server.init();
        while (g_running)
            server.tick();
        server.stop();
        std::cout << "---------------------\nwebserv shut down cleanly\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << "Fatal: " << e.what() << "\n";
        // ~Server() destructor handles fd cleanup
        return 1;
    }

    return 0;
}
