#pragma once

#include <map>
#include <vector>
#include <sys/epoll.h>  // epoll_create1, epoll_ctl, epoll_wait, epoll_event: can handle 100,000+ fds, O(1) time complexity, but only on linux
#include <sys/socket.h> // socket(), bind(), listen(), accept()
#include <netinet/in.h> // sockaddr_in
#include <arpa/inet.h>  // inet_addr()
#include <unistd.h>     // close()
#include <fcntl.h>      // fcntl() non-blocking
// #include "ServerConfig.hpp"
#include "HttpRequest.hpp"
#include "Client.hpp"
#include "StaticFileHandler.hpp"
#include "ConnectionManager.hpp"
#include "EpollLoop.hpp"
#include "ProcessRequest.hpp"

/*
* Owns one listening socket (fd).
* Uses epoll() to multiplex the listen fd + all client fds.
*
* Lifecycle:
*   Server srv(config);
*   srv.init();   // socket → setsockopt → bind → listen → fcntl non-block
*   srv.tick();    // poll loop — blocks until SIGINT or error
*/

class Server {
	public:
		explicit Server(const std::vector<ServerConfig> &configs);
		Server(const Server &) = delete; // no cpy or assign
		Server &operator=(const Server &) = delete;
		~Server();

		void init();    // call once — socket, bind, listen, epoll setup
		void tick();    // call in a loop — ONE epoll_wait iteration
		void stop();    // sets _running = false

	private:
		void			_acceptClient();
		static void	 	_setNonBlocking(int fd);
		void 			handleServerTimeout();

		// named constants for server tuning
		enum {
			BACKLOG      = 128,   // max queued incoming connections waiting to be accepted; named so to match exact socket API term listen(fd, backlog), Kernel docs/man pages call it “backlog”
			POLL_TIMEOUT = 100,  // ms —  how often to check for shutdown signal (SIGINT) in main loop; if too long, server may be slow to respond to shutdown; if too short, may cause more CPU wakeups and slightly higher CPU usage when idle
			maxEvents   = 64,   // max ready events handled per tick call
			READ_BUF     = 4096, // chunk size per recv
			SERVER_TIMEOUT = 6 // for testing: usually is 60 seconds of idle time before server closes client connection
		};

		int                     _listen_fd;
		bool                    _running;
		std::vector<ServerConfig> _configs;
		EpollLoop               _epoll;
		std::map<int, Client *> _clients;
		ProcessRequest          _process_request;
		ConnectionManager      _connection_manager;
};