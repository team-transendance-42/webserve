#include <sys/stat.h>   // stat()
#include "../includes/Server.hpp"

// ── ctor / dtor ───────────────────────────────────────────────────────────────

Server::Server(const ServerConfig &config)
    : _config(config), _listenFd(-1), _epollFd(-1), _running(true) {
		// todo: missing init _clients
	}

Server::~Server() {
    typedef std::map<int, Client *>::iterator It;
    for (It it = _clients.begin(); it != _clients.end(); ++it) {
        close(it->first);
        delete it->second;// pointer to heap obj created by new in _acceptClient, we need to free the memory
    }
    if (_listenFd >= 0) close(_listenFd);
    if (_epollFd  >= 0) close(_epollFd);
}

// ── init ──────────────────────────────────────────────────────────────────────

void Server::init() {
    // 1. epoll instance
    _epollFd = epoll_create1(0);
    if (_epollFd < 0)
        throw std::runtime_error("epoll_create1() failed: "
            + std::string(strerror(errno)));

    // 2. TCP socket
    _listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (_listenFd < 0)
        throw std::runtime_error("socket() failed: "
            + std::string(strerror(errno)));

    // 3. SO_REUSEADDR — no "address already in use" on restart
    int opt = 1;
    if (setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        throw std::runtime_error("setsockopt() failed");

    // 4. bind
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr)); // todo: do we need it? memory....
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(_config.port);
    addr.sin_addr.s_addr = inet_addr(_config.host.c_str());

    if (bind(_listenFd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        throw std::runtime_error("bind() failed on "
            + _config.host + ":" + std::to_string(_config.port)
            + " — " + strerror(errno));

    // 5. listen
    if (listen(_listenFd, BACKLOG) < 0)
        throw std::runtime_error("listen() failed: "
            + std::string(strerror(errno)));

    // 6. non-blocking
    _setNonBlocking(_listenFd);

    // 7. register with epoll
    _epollAdd(_listenFd, EPOLLIN);

    std::cout << "[Server] '" << _config.server_names[0]
              << "' listening on " << _config.host
              << ":" << _config.port << "\n";
}

// ── tick ───────────────────────────────────────────────────────────────────────

/**
 *  EPOLLERR: An error occurred on the file descriptor (e.g., socket error).
	EPOLLHUP: The file descriptor was "hung up" (connection closed by peer).
	EPOLLRDHUP: The peer closed its read end (remote shutdown).
	The & operator in this context is a bitwise AND. It checks if any of the specified event flags (EPOLLERR, EPOLLHUP, EPOLLRDHUP) are set in ev.

	If ev contains any of those flags, the result is non-zero, so the condition is true. This is a common way to test for specific bits in a bitmask.
 */

void Server::tick() {
    struct epoll_event events[MAX_EVENTS];

    // ONE epoll_wait call — returns after POLL_TIMEOUT ms if nothing happens
    // this lets main() check g_running regularly
	// numReadyEvents = num of fds ready for i/o: how many events are avail in the events arr
    int numReadyEvents = epoll_wait(_epollFd, events, MAX_EVENTS, POLL_TIMEOUT);

    if (numReadyEvents < 0) {
        if (errno == EINTR) return;  // signal interrupted — main will check g_running
        throw std::runtime_error("epoll_wait() failed: "
            + std::string(strerror(errno)));
    }
    if (numReadyEvents == 0) return;  // timeout — nothing ready, return to main

    for (int i = 0; i < numReadyEvents; i++) {
        int      fd = events[i].data.fd;
        uint32_t ev = events[i].events;

        if (fd == _listenFd) {
            _acceptClient();
            continue;
        }

        auto it = _clients.find(fd);
        if (it == _clients.end()) continue;
        Client &client = *it->second;

        if (ev & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
            _closeClient(fd);
            continue;
        }
        if (ev & EPOLLIN)  _readClient(client);
        if (ev & EPOLLOUT) _writeClient(client);
    }
}

void Server::stop() {
    _running = false;
    std::cout << "[Server] stopping '" << _config.server_names[0] << "'\n";
}

// ── epoll helpers ─────────────────────────────────────────────────────────────

void Server::_epollAdd(int fd, uint32_t events) {
    struct epoll_event ev;
    ev.events  = events;
    ev.data.fd = fd;
    if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, fd, &ev) < 0)
        throw std::runtime_error("epoll_ctl ADD failed: "
            + std::string(strerror(errno)));
}

void Server::_epollMod(int fd, uint32_t events) {
    struct epoll_event ev;
    ev.events  = events;
    ev.data.fd = fd;
    if (epoll_ctl(_epollFd, EPOLL_CTL_MOD, fd, &ev) < 0)
        throw std::runtime_error("epoll_ctl MOD failed: "
            + std::string(strerror(errno)));
}

void Server::_epollDel(int fd) {
    if (epoll_ctl(_epollFd, EPOLL_CTL_DEL, fd, NULL) < 0)
        std::cerr << "[epoll] DEL failed fd=" << fd
                  << ": " << strerror(errno) << "\n";
}

// ── _acceptClient ─────────────────────────────────────────────────────────────

void Server::_acceptClient() {
    while (true) {
        struct sockaddr_in addr;
        socklen_t len = sizeof(addr);

        int clientFd = accept(_listenFd, (struct sockaddr *)&addr, &len);
        if (clientFd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            std::cerr << "[accept] failed: " << strerror(errno) << "\n";
            break;
        }

        _setNonBlocking(clientFd);
        _epollAdd(clientFd, EPOLLIN | EPOLLRDHUP);
        _clients[clientFd] = new Client(clientFd);

        std::cout << "[Server] new client fd=" << clientFd << "\n";
    }
}

// ── _readClient ───────────────────────────────────────────────────────────────

void Server::_readClient(Client &client) {
    char buf[READ_BUF];

    while (true) {
        std::memset(buf, 0, sizeof(buf));
        ssize_t bytes = recv(client.fd, buf, sizeof(buf) - 1, 0);

        if (bytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            _closeClient(client.fd);
            return;
        }
        if (bytes == 0) {
            _closeClient(client.fd);
            return;
        }

        ParseResult result = client.request.feed(buf, bytes);

        if (result == PARSE_ERROR) {
            client.write_buf  = HttpResponse::make_400().serialize();
            client.keep_alive = false;
            _epollMod(client.fd, EPOLLOUT | EPOLLRDHUP);
            return;
        }

        if (result == COMPLETE) {
            _processRequest(client);
            // switch to write mode — response is ready
            _epollMod(client.fd, EPOLLOUT | EPOLLRDHUP);
            return;
        }
        // INCOMPLETE → keep reading
    }
}

// ── _writeClient ──────────────────────────────────────────────────────────────

void Server::_writeClient(Client &client) {
    while (!client.write_buf.empty()) {
        ssize_t sent = send(client.fd,
                            client.write_buf.c_str(),
                            client.write_buf.size(), 0);
        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return;
            _closeClient(client.fd);
            return;
        }
        client.write_buf.erase(0, sent);
    }

    // response fully sent
    if (client.keep_alive) {
        client.request.clear();          // reset parser for next request
        _epollMod(client.fd, EPOLLIN | EPOLLRDHUP);  // back to read mode
    } else {
        _closeClient(client.fd);
    }
}

// ── _closeClient ──────────────────────────────────────────────────────────────

void Server::_closeClient(int fd) {
    _epollDel(fd);
    close(fd);

    std::map<int, Client *>::iterator it = _clients.find(fd);
    if (it != _clients.end()) {
        delete it->second;
        _clients.erase(it);
    }
    std::cout << "[Server] client closed fd=" << fd << "\n";
}

// ── _processRequest ───────────────────────────────────────────────────────────

void Server::_processRequest(Client &client) {
    HttpRequest &req = client.request;
    req.debug_print();
    client.keep_alive = req.is_keep_alive();

    // ── 1. method check ───────────────────────────────────────────────────
    if (req.method == UNKNOWN) {
        client.write_buf = HttpResponse::make_400().serialize();
        return;
    }

    // ── 2. match location ─────────────────────────────────────────────────
    const Location *loc = _config.matchLocation(req.path);
    if (!loc) {
        client.write_buf = HttpResponse::make_404().serialize();
        return;
    }

    // ── 2.5 forbidden access ──────────────────────────────────────────────
    if (loc->deny_all == true) {
        std::map<int,std::string>::const_iterator ep = _config.error_pages.find(403);
        if (ep != _config.error_pages.end())
            client.write_buf = StaticFileHandler::serveStatic(ep->second).serialize();
        else
            client.write_buf = HttpResponse::make_403().serialize();
        return;
    }

    // ── 3. allowed methods ────────────────────────────────────────────────
    const char *method_str[] = { "GET", "POST", "DELETE" };
    std::string req_method   = method_str[req.method];
    bool method_ok = false;
    for (size_t i = 0; i < loc->allowed_methods.size(); i++) {
        if (loc->allowed_methods[i] == req_method) { method_ok = true; break; }
    }
    if (!method_ok) {
        client.write_buf = HttpResponse::make_405().serialize();
        return;
    }

    // ── 4. body size ──────────────────────────────────────────────────────
    long max_body = (loc->client_max_body_size >= 0)
                    ? loc->client_max_body_size
                    : _config.client_max_body_size;
    if ((long)req.body.size() > max_body) {
        client.write_buf = HttpResponse::make_413().serialize();
        return;
    }

    // ── 5. redirect ───────────────────────────────────────────────────────
    if (loc->redirect_code != 0) {
        client.write_buf = (loc->redirect_code == 301)
            ? HttpResponse::make_301(loc->redirect_url).serialize()
            : HttpResponse::make_302(loc->redirect_url).serialize();
        return;
    }

    // ── 6. build filesystem path ──────────────────────────────────────────
    // root + request path, e.g. "./www/one" + "/index.html"
    std::string root     = loc->root;
    std::string url_path = req.path;

    // strip trailing slash for stat, re-add for dir logic
    std::string filepath;
    if (url_path == loc->path) {
        // If the request matches the location exactly, serve the index file
        filepath = root + "/" + loc->index;
    } else {
        // Otherwise, append the rest of the path
        filepath = root + url_path.substr(loc->path.length());
    }
    

    // ── 7. stat the path ──────────────────────────────────────────────────
    struct stat st;
    if (stat(filepath.c_str(), &st) != 0) {
        if (errno == EACCES) {
            std::map<int,std::string>::const_iterator ep = _config.error_pages.find(403);
            if (ep != _config.error_pages.end())
                client.write_buf = StaticFileHandler::serveStatic(ep->second).serialize();
            else
                client.write_buf = HttpResponse::make_403().serialize();
            return;
        }
        // check configured error page
        std::map<int,std::string>::const_iterator ep
            = _config.error_pages.find(404);
        if (ep != _config.error_pages.end())
            client.write_buf = StaticFileHandler::serveStatic(ep->second).serialize();
        else
            client.write_buf = HttpResponse::make_404().serialize();
        return;
    }

    // ── 8. directory handling ─────────────────────────────────────────────
    if (S_ISDIR(st.st_mode)) {
        // try index file first
        std::string index_path = filepath;
        if (index_path[index_path.size()-1] != '/') index_path += '/';
        index_path += loc->index;

        struct stat ist;
        if (stat(index_path.c_str(), &ist) == 0 && S_ISREG(ist.st_mode)) {
            client.write_buf = StaticFileHandler::serveStatic(index_path).serialize();
            return;
        }

        // autoindex
        if (loc->autoindex) {
            client.write_buf = StaticFileHandler::autoindex(filepath, url_path).serialize();
            return;
        }

        client.write_buf = HttpResponse::make_403().serialize();
        return;
    }

    // ── 9. serve regular file ─────────────────────────────────────────────
    client.write_buf = StaticFileHandler::serveStatic(filepath).serialize();
}

// ── utils ─────────────────────────────────────────────────────────────────────

void Server::_setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        throw std::runtime_error("fcntl F_GETFL failed");
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        throw std::runtime_error("fcntl F_SETFL failed");
}

