// main.cpp
#include "includes/ServerConfig.hpp"
#include "includes/Server.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <csignal>

/*The variable keeps its value for the lifetime of the program (static).
The compiler must always actually read/write the variable’s value, not cache it in a register (volatile, cpu fast storage), because it might change outside the normal program flow (e.g., in a signal handler).*/
static volatile sig_atomic_t g_running = 1;

static void onSignal(int) { g_running = 0; }


// make -j4 runs up to 4 build jobs in parallel

int main(int argc, char *argv[])
{
    if (argc > 2) {
        std::cerr << "Usage: ./webserv [config_file.conf]\n"
                  << "       or leave empty to use default.conf\n";
        return 1;
    }

    // Signal handling — graceful shutdown on Ctrl+C / SIGTERM
    struct sigaction sa{};
    sa.sa_handler = onSignal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    std::string configFile = (argc == 2) ? argv[1] : "default.conf";

    try
    {
        // TODO: swap createDefaultServerConfigs() for parseConfigFile(configFile) when parser is ready
        std::vector<ServerConfig> cfgs = createDefaultServerConfigs();

        // Group configs by (host, port) — one Server (one listen socket) per unique address.
        // Configs sharing an address become virtual hosts; Host: header selects among them.
        typedef std::pair<std::string, int> AddrKey;
        std::map<AddrKey, std::vector<ServerConfig> > groups;
        for (size_t i = 0; i < cfgs.size(); ++i)
            groups[std::make_pair(cfgs[i].host, cfgs[i].port)].push_back(cfgs[i]);

        std::vector<Server *> servers;
        for (std::map<AddrKey, std::vector<ServerConfig> >::iterator it = groups.begin();
             it != groups.end(); ++it)
            servers.push_back(new Server(it->second));
        for (size_t i = 0; i < servers.size(); ++i)
            servers[i]->init();

        while (g_running)
            for (size_t i = 0; i < servers.size(); ++i)
                servers[i]->tick();

        for (size_t i = 0; i < servers.size(); ++i) {
            servers[i]->stop();
            delete servers[i];
        }
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
