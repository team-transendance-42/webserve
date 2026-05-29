#include "includes/EventLoop.hpp"
#include "includes/Listener.hpp"

#include <csignal>
#include <iostream>
#include <map>
#include <string>
#include <vector>

/* The variable keeps its value for the lifetime of the program (static).
 * volatile so the signal handler's write is always observed. */
static volatile sig_atomic_t g_running = 1;

static void onSignal(int) { g_running = 0; }

int main(int argc, char *argv[])
{
    if (argc > 2) {
        std::cerr << "Usage: ./webserv [config_file.conf]\n"
                  << "       or leave empty to use tests/conf/default.conf\n";
        return (1);
    }

    /* SIGPIPE → SIG_IGN: send() to a closed peer returns -1/EPIPE instead of
     * killing the process. ConnectionManager::writeClient closes on sent <= 0. */
    struct sigaction sa{};
    sa.sa_handler = onSignal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, nullptr);

    std::string configFile = (argc == 2) ? argv[1] : "tests/conf/default.conf";
    std::cout << "--- webserv — loading " << configFile << " ---\n";

    std::vector<Listener *> listeners;
    try {
        ConfigParser parser;
        ConfigFile config = parser.parseFile(configFile);
        if (config.servers.empty())
            throw std::runtime_error("no servers defined in config");

        /* Group configs by (host, port). One Listener(Server) per unique listen address;
         * configs sharing an address become virtual hosts behind that Listener. */
        typedef std::pair<std::string, int> AddrKey; // host, port
        std::map<AddrKey, std::vector<ServerConfig> > groups;
        for (size_t i = 0; i < config.servers.size(); ++i)
            groups[std::make_pair(config.servers[i].host, config.servers[i].port)]
                .push_back(config.servers[i]);

        EventLoop loop;
        loop.init();

        for (std::map<AddrKey, std::vector<ServerConfig> >::iterator it = groups.begin();
             it != groups.end(); ++it) {
            Listener *l = new Listener(it->second);
            listeners.push_back(l);
            l->init();
            loop.addListener(l);
        }

        loop.run(g_running);

        for (size_t i = 0; i < listeners.size(); ++i) delete listeners[i];
        std::cout << "---------------------\nwebserv shut down cleanly\n";
    } catch (const std::exception &e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        for (size_t i = 0; i < listeners.size(); ++i) delete listeners[i];
        return (1);
    }

    return (0);
}
