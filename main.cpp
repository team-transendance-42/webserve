#include "includes/config/Config.hpp"

#include <exception>
#include <iostream>

int main(int argc, char** argv) {
	if (argc > 2) {
		std::cerr << "Usage: ./webserv [config.conf]\n";
		return 1;
	}

	std::string configPath = "srcs/config/default/default.conf";
	if (argc == 2) {
		configPath = argv[1];
	}

	try {
		ConfigParser parser;
		ConfigFile config = parser.parseFile(configPath);

		std::cout << "Parsed " << config.servers.size() << " server block(s) from " << configPath << "\n";
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
			if (!server.locations.empty()) {
				const Location& location = server.locations[0];
				std::cout << "      first location: " << location.path
						  << " (" << location.allowed_methods.size() << " allowed method(s))\n";
			}
		}
	}
	catch (const std::exception& ex) {
		std::cerr << ex.what() << "\n";
		return 1;
	}

	return 0;
}