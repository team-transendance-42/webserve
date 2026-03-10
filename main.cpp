#include "ConfigParser.hpp"

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
			std::cout << "  [" << i << "] "
					  << server.server_name
					  << " (" << server.host << ":" << server.listen << ")"
					  << " with " << server.locations.size() << " location(s)\n";
		}
	}
	catch (const std::exception& ex) {
		std::cerr << ex.what() << "\n";
		return 1;
	}

	return 0;
}