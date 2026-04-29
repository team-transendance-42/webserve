// main.cpp
#include "includes/ServerConfig.hpp"
#include "includes/Server.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <csignal>

/*The variable keeps its value for the lifetime of the program (static).
The compiler must always actually read/write the variable’s value, not cache it in a register (volatile), because it might change outside the normal program flow (e.g., in a signal handler).*/
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
