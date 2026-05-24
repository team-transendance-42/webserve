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
                  << "       or leave empty to use tests/conf/default.conf\n";
        return (1);
    }

    /*Signal handling — graceful shutdown on Ctrl+C / SIGTERM
    SIGPIPE is a Unix signal sent to a process when it tries to write to a
   socket/pipe whose read end is already closed — i.e., the client
  disconnected before the server finished sending the response.
  Default behavior: the OS kills the process immediately. No error
  code, no exception — just dead server.
   By setting SIG_IGN, we tell the kernel "don't kill me, just return an
  error." Now when send() writes to a gone client it returns -1 with
  errno = EPIPE. Your existing code in ConnectionManager::writeClient
  already handles sent <= 0 → closeClient(fd), so the dead connection is
   cleaned up gracefully and the server keeps running.
    */

    struct sigaction sa{};
    sa.sa_handler = onSignal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, nullptr);

    std::string configFile = (argc == 2) ? argv[1] : "tests/conf/default.conf";

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
