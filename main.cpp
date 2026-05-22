#include "includes/Server.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <csignal>
#include <memory>

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
        return (1);
    }

    // Signal handling — graceful shutdown on Ctrl+C / SIGTERM
    struct sigaction sa{};
    sa.sa_handler = onSignal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
	/*
	When the server calls send() on a socket and the client has already closed their end, the OS sends the process a signal named SIGPIPE ("broken
   	pipe").
	Signals are asynchronous notifications from the OS. Each signal has a default action. For SIGPIPE, the default action is: terminate the process 
	immediately.
	This is sensible for command-line pipes (cat file | grep foo) but catastrophic for a server — one disconnecting client should never kill the
	whole server.
	---
	When a client disconnected mid-write, send() triggered SIGPIPE. The OS default action for SIGPIPE is process termination — one bad client killed
	the entire server.
	SIG_IGN tells the OS: instead of terminating, just make send() return -1 with errno == EPIPE.
	*/
	sa.sa_handler = SIG_IGN; // ignore signal on disconnected client
	sigaction(SIGPIPE, &sa, nullptr);

    std::string configFile = (argc == 2) ? argv[1] : "default.conf";

    std::cout << "--- webserv — loading " << configFile << " ---\n";
	std::vector<Server *> servers;
    try {
        // Parse config file into ConfigFile structure
        ConfigParser parser;
        ConfigFile config = parser.parseFile(configFile);
        if (config.servers.empty()) {
            throw std::runtime_error("no servers defined in config");
        }

        // Group configs by (host, port) — one Server (one listen socket) per unique address.
        typedef std::pair<std::string, int> AddrKey;
        std::map<AddrKey, std::vector<ServerConfig> > groups;
        for (size_t i = 0; i < config.servers.size(); ++i)
            groups[std::make_pair(config.servers[i].host, config.servers[i].port)].push_back(config.servers[i]);

        for (std::map<AddrKey, std::vector<ServerConfig> >::iterator it = groups.begin();
            it != groups.end(); ++it) {
				Server* s = new Server(it->second);
				servers.push_back(s);
				s->init();
            }

        while (g_running)
            for (size_t i = 0; i < servers.size(); ++i)
                servers[i]->tick();

        for (size_t i = 0; i < servers.size(); ++i) {
            delete servers[i];
        }
        std::cout << "---------------------\nwebserv shut down cleanly\n";
    } catch (const std::exception &e) {
        std::cerr << "Fatal: " << e.what() << "\n";
		for (size_t i = 0; i < servers.size(); ++i) delete servers[i];
        return (1);
    }

    return (0);
}
