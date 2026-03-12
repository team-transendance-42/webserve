#pragma once

#include <string>
#include <vector>
#include <map>
#include <sys/epoll.h>  // epoll_create1, epoll_ctl, epoll_wait, epoll_event: can handle 100,000+ fds, O(1) time complexity
#include <sys/socket.h> // socket(), bind(), listen(), accept()
#include <netinet/in.h> // sockaddr_in
#include <arpa/inet.h>  // inet_addr()
#include <unistd.h>     // close()
#include <fcntl.h>      // fcntl() non-blocking
#include <dirent.h>   // opendir/readdir for autoindex
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <cerrno>
#include <cstring>      // strerror()
#include "ServerConfig.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "Client.hpp"

/*
** Server
** ------
** Owns one listening socket (fd).
** Uses epoll() to multiplex the listen fd + all client fds.
**
** Lifecycle:
**   Server srv(config);
**   srv.init();   // socket → setsockopt → bind → listen → fcntl non-block
**   srv.tick();    // poll loop — blocks until SIGINT or error
*/

class Server {
	public:
		explicit Server(const ServerConfig &config);
		Server(const Server &) = delete; // no cpy or assign
		Server &operator=(const Server &) = delete;
		~Server();

		void init();    // call once — socket, bind, listen, epoll setup
		void tick();    // call in a loop — ONE epoll_wait iteration
		void stop();    // sets _running = false

		int  getFd()     const { return _listenFd; }
		bool isRunning() const { return _running;  }

	private:
		void _epollAdd(int fd, uint32_t events);
		void _epollMod(int fd, uint32_t events);
		void _epollDel(int fd);

		void 				_acceptClient();
		void 				_readClient  (Client &client);
		void 				_writeClient (Client &client);
		void 				_closeClient (int fd);
		void 				_processRequest(Client &client);
		HttpResponse        _autoindex    (const std::string &dirpath,
                                   const std::string &url_path);

		static HttpResponse _serveStatic(const std::string &filepath);
		static std::string  _mimeType   (const std::string &path);
		static void        _setNonBlocking(int fd);
		static std::string _itoa(int n);

		enum {
			BACKLOG      = 128,
			POLL_TIMEOUT = 100,   // ms — short so main loop checks g_running often
			MAX_EVENTS   = 64,
			READ_BUF     = 4096
		};

		ServerConfig            _config;
		int                     _listenFd;
		int                     _epollFd;
		bool                    _running;
		std::map<int, Client *> _clients;
};