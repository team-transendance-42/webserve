// main.cpp
#include "includes/ServerConfig.hpp"
#include "includes/Server.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <csignal>
#include <memory>

static volatile sig_atomic_t g_running = 1;

static void onSignal(int) { g_running = 0; }


// make -j4 runs up to 4 build jobs in parallel: do it instead of make
/**
 * curl -v -X POST http://127.0.0.1:8080/ \
  -H "Content-Type: text/plain" \
  -d "hello from curl"

  curl -v -X POST http://127.0.0.1:8080/ \
  -H "Content-Type: application/json" \
  -d '{"name":"pekatsar","msg":"hi"}'

  curl -v -X POST http://127.0.0.1:8080/ \
  -H "Content-Type: application/x-www-form-urlencoded" \
  -d "name=pekatsar&age=42"
 */
int main(int argc, char *argv[])
{
    if (argc > 2) {
        std::cout << "Naughty, naughty: you can enter only one filename.conf"
                     " or leave empty for default.conf\n";
        return (0);
    }

    // Signal handling — graceful shutdown on Ctrl+C / SIGTERM
    struct sigaction sa{};
    sa.sa_handler = onSignal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    // Config file from argv or default
    std::string configFile = (argc == 2) ? argv[1] : "default.conf";

    std::cout << "--- webserv — loading " << configFile << " ---\n";

    try
    {
        ConfigParser parser;
        ConfigFile config = parser.parseFile(configFile);
        if (config.servers.empty()) {
            throw std::runtime_error("no servers defined in config");
        }

        // Debug print of parsed config summary
        std::cout << "Parsed " << config.servers.size() << " server block(s) from " << configFile << "\n";
		for (std::size_t i = 0; i < config.servers.size(); ++i) {
			const ServerConfig& server = config.servers[i];
			std::string primaryServerName = "(no server_name)";
			if (!server.server_names.empty()) {
				primaryServerName = server.server_names[0];
			}
			std::cout << "  [" << i << "] "
					  << primaryServerName
					  << " (" << server.host << ":" << server.port << ")"
					  << " with " << server.locations.size() << " location(s)\n";
            // print all locations for this server
            for (const auto& loc : server.locations) {
                std::cout << "      - " << loc.path << "\n";
            }
		}

        // Start a Server instance for each server block in config
        std::vector<std::unique_ptr<Server> > servers;
        servers.reserve(config.servers.size());

        // Init and run servers until signal to stop, then clean up
        for (std::size_t i = 0; i < config.servers.size(); ++i) {
            const ServerConfig& cfg = config.servers[i];
            std::unique_ptr<Server> server(new Server(cfg));
            server->init();
            servers.push_back(std::move(server));
        }

        while (g_running) {
            for (std::size_t i = 0; i < servers.size(); ++i) {
                servers[i]->tick();
            }
        }

        for (std::size_t i = 0; i < servers.size(); ++i) {
            servers[i]->stop();
        }

        std::cout << "---------------------\nwebserv shut down cleanly\n";
    } catch (const std::exception &e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        // ~Server() destructor handles fd cleanup
        return (1);
    }

    return (0);
}
