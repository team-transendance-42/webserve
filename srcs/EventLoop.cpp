#include "../includes/EventLoop.hpp"

#include <cerrno>
#include <csignal>
#include <cstring>
#include <ctime>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <sys/epoll.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../includes/ErrorResponseBuilder.hpp"
#include "../includes/HttpResponse.hpp"
#include "../includes/Listener.hpp"
#include "../includes/CgiExecutor.hpp"

/*
 * Constructor: wires ConnectionManager with epoll callbacks pointing at the
 * single shared EpollLoop. The unified _clients map is passed by reference so
 * accept (here) and close (inside ConnectionManager) mutate the same registry.
 */
EventLoop::EventLoop()
    : _epoll(),
      _conn(_clients, _clientToListener, [this](int fd, uint32_t events) {
                if (!_epoll.mod(fd, events)) {
                    int err = errno;
                    std::cerr << "[EventLoop] epoll MOD fd=" << fd
                              << " (" << strerror(err) << ") — connection will time out\n";
                }
            }, [this](int fd) { _epoll.del(fd); },
            [this](Client &client) { this->registerCgiPipes(client); },
            [this](Client &client) { this->cleanupCgiOnClientClose(client); }) {}

EventLoop::~EventLoop() {
    for (std::map<int, Client *>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        close(it->first);
        delete it->second;
    }
}

void EventLoop::init() {
    _epoll.init();
}

/*
 * Register a Listener's listen socket with the shared epoll. EPOLLIN fires
 * whenever a new client is queued; tick() dispatches it back to _acceptFrom.
 */
void EventLoop::addListener(Listener *listener) {
    int lfd = listener->listenFd();
    if (lfd < 0)
        throw std::runtime_error("EventLoop::addListener: listener not initialised");
    _listenFds[lfd] = listener;
    if (!_epoll.add(lfd, EPOLLIN)) {
        int err = errno;
        throw std::runtime_error("EventLoop::addListener: epoll ADD failed for listen fd="
                                 + std::to_string(lfd) + " (" + strerror(err) + ")");
    }
}

void EventLoop::run(volatile sig_atomic_t &running) {
    while (running) tick();
}

/* Register CGI pipes for an active session with the shared epoll.
   Both stdin_fd (for body writes) and stdout_fd (for output reads) are registered. */
void EventLoop::registerCgiPipes(Client &client) {
    if (!client.cgi) return;

    CgiSession &session = *client.cgi;

    /* Register stdin for writing (writing body chunks) */
    if (session.stdin_fd >= 0) {
        if (!_epoll.add(session.stdin_fd, EPOLLOUT)) {
            std::cerr << "[EventLoop] Failed to register CGI stdin fd=" << session.stdin_fd << "\n";
            kill(session.pid, SIGKILL);
            _cleanupCgiSession(client);
            return;
        }
        _cgiPipeToClient[session.stdin_fd] = &client;
    }

    /* Register stdout for reading (reading output) */
    if (session.stdout_fd >= 0) {
        if (!_epoll.add(session.stdout_fd, EPOLLIN | EPOLLHUP)) {
            std::cerr << "[EventLoop] Failed to register CGI stdout fd=" << session.stdout_fd << "\n";
            kill(session.pid, SIGKILL);
            _cleanupCgiSession(client);
            return;
        }
        _cgiPipeToClient[session.stdout_fd] = &client;
    }
}

/* Clean up CGI session when client is closing (mid-CGI disconnect). */
void EventLoop::cleanupCgiOnClientClose(Client &client) {
    if (!client.cgi) return;

    CgiSession &session = *client.cgi;

    /* Kill the CGI process if still running */
    if (session.pid > 0) {
        kill(session.pid, SIGKILL);
        /* Optionally wait to reap, but we rely on the idle sweep on next tick as safety net */
    }

    /* Clean up pipes and session */
    _cleanupCgiSession(client);
}

/*
 * One epoll_wait iteration. Four dispatch buckets:
 *   1. fd is a listen socket → drain accept() until EAGAIN.
 *   2. fd is a CGI pipe (stdin/stdout) → handle CGI I/O events.
 *   3. fd is a known client socket → readClient / writeClient / closeClient by event mask.
 *   4. fd is unknown → silently skip (closed mid-batch).
 *
 * Idle clients and expired CGI sessions are swept once per tick.
 */
void EventLoop::tick() {
    struct epoll_event events[MAX_EVENTS];
    int n = _epoll.wait(events, MAX_EVENTS, POLL_TIMEOUT_MS);

    if (n < 0) {
        if (errno == EINTR) return;
        throw std::runtime_error("epoll_wait() failed: " + std::string(strerror(errno)));
    }

    _closeIdleClients();

    if (n == 0) return;

    for (int i = 0; i < n; ++i) {
        int      fd = events[i].data.fd;
        uint32_t ev = events[i].events;

        /* Check if it's a listen socket */
        std::map<int, Listener *>::iterator lit = _listenFds.find(fd);
        if (lit != _listenFds.end()) {
            _acceptFrom(lit->second);
            continue;
        }

        /* Check if it's a CGI pipe fd */
        std::map<int, Client *>::iterator cpipe = _cgiPipeToClient.find(fd);
        if (cpipe != _cgiPipeToClient.end()) {
            Client *client = cpipe->second;
            if (client && client->cgi) {
                _handleCgiPipeEvent(fd, ev);
            }
            continue;
        }

        /* Check if it's a client socket */
        std::map<int, Client *>::iterator cit = _clients.find(fd);
        if (cit == _clients.end()) continue;
        Client &client = *cit->second;

        if (ev & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
            _conn.closeClient(fd);
            continue;
        }
        if (ev & EPOLLIN) {
            _conn.readClient(client, READ_BUF);
            /* readClient may have closed and deleted the client (disconnect, parse error).
               Re-check before touching client again — use-after-free otherwise. */
            if (_clients.find(fd) == _clients.end()) continue;
        }
        if (ev & EPOLLOUT) _conn.writeClient(client);
    }
}

/*
 * Drain the listen socket's pending backlog. Each accepted fd is registered
 * in the shared epoll and bound to the originating Listener so per-client
 * routing (config lookup, ProcessRequest) follows the connection.
 */
void EventLoop::_acceptFrom(Listener *listener) {
    while (true) {
        int cfd = listener->acceptOne();
        if (cfd < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK)
                std::cerr << "[EventLoop] accept failed on listen_fd="
                          << listener->listenFd() << " (" << strerror(errno) << ")\n";
            break;
        }
        Client *c = new Client(cfd); // allocate before epoll.add so fd is never in epoll without a Client owner
        if (!_epoll.add(cfd, EPOLLIN | EPOLLRDHUP)) {
            int err = errno;
            std::cerr << "[EventLoop] epoll ADD failed for new client fd=" << cfd
                      << " (" << strerror(err) << ") — dropping connection\n";
            close(cfd);
            delete c;
            continue;
        }
        _clients[cfd] = c;
        _clientToListener[cfd] = listener;
        std::cout << "[EventLoop] new client fd=" << cfd
                  << " on listen_fd=" << listener->listenFd() << "\n";
    }
}

/* Sends 408 to clients idle past CLIENT_TIMEOUT and arms them for EPOLLOUT.
   Also checks for expired CGI sessions and kills them with 504 Gateway Timeout.
   On epoll MOD failure: collect the fd and close after the loop —
   closing inside the loop would invalidate the _clients iterator. */
void EventLoop::_closeIdleClients() {
    time_t now = std::time(nullptr);
    std::vector<int> toForceClose;
    for (std::map<int, Client *>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        Client *client = it->second;

        /* Check for expired CGI session (gateway timeout) */
        if (client->cgi && now >= client->cgi->deadline) {
            std::cout << "[EventLoop] CGI timeout: closing client fd=" << client->fd << "\n";
            kill(client->cgi->pid, SIGKILL);
            _cleanupCgiSession(*client);
            
            /* Build 504 Gateway Timeout response */
            std::map<int, Listener *>::iterator listenerIt = _clientToListener.find(client->fd);
            if (listenerIt != _clientToListener.end()) {
                client->writeBuf = HttpResponse::make_504().serialize();
                HttpResponse::injectConnectionHeader(client->writeBuf, false);
            } else {
                client->writeBuf = HttpResponse::make_504().serialize();
            }
            client->keep_alive = false;
            if (!_epoll.mod(client->fd, EPOLLOUT | EPOLLRDHUP)) {
                int err = errno;
                std::cerr << "[EventLoop] epoll MOD failed for CGI timeout fd=" << client->fd
                          << " (" << strerror(err) << ") — force closing\n";
                toForceClose.push_back(client->fd);
            }
            continue;
        }

        /* Check for idle client timeout (only if no CGI is running) */
        if (!client->cgi && now - client->lastTimestamp > CLIENT_TIMEOUT && client->writeBuf.empty()) {
            std::map<int, Listener *>::iterator listenerIt = _clientToListener.find(client->fd);
            if (listenerIt == _clientToListener.end()) {
                toForceClose.push_back(client->fd);
                continue;
            }
            const ServerConfig &cfg = listenerIt->second->configs()[0];
            std::cout << "[EventLoop] timeout: closing client fd=" << client->fd << "\n";
            client->writeBuf   = ErrorResponseBuilder::buildErrorResponse(408, cfg).serialize();
            client->keep_alive = false;
            HttpResponse::injectConnectionHeader(client->writeBuf, false);
            if (!_epoll.mod(client->fd, EPOLLOUT | EPOLLRDHUP)) {
                int err = errno;
                std::cerr << "[EventLoop] epoll MOD failed for timeout fd=" << client->fd
                          << " (" << strerror(err) << ") — force closing\n";
                toForceClose.push_back(client->fd);
            }
        }
    }
    for (size_t i = 0; i < toForceClose.size(); ++i)
        _conn.closeClient(toForceClose[i]);
}

/* Handle I/O events on a CGI pipe (stdin for writing body, stdout for reading output).
   - EPOLLOUT on stdin_fd: Write next chunk of body; close stdin when complete.
   - EPOLLIN on stdout_fd: Drain available data into output buffer.
   - EPOLLHUP on stdout_fd: Process has closed stdout; finalize the CGI session.
*/
void EventLoop::_handleCgiPipeEvent(int fd, uint32_t events) {
    std::map<int, Client *>::iterator it = _cgiPipeToClient.find(fd);
    if (it == _cgiPipeToClient.end()) return;

    Client *client = it->second;
    if (!client || !client->cgi) return;

    CgiSession &session = *client->cgi;

    /* EPOLLOUT: write next chunk of body to stdin */
    if ((events & EPOLLOUT) && fd == session.stdin_fd && session.state == CgiSession::STATE_BODY_WRITE) {
        std::size_t remaining = session.body.size() - session.body_written;
        if (remaining > 0) {
            ssize_t n = write(fd, session.body.data() + session.body_written, remaining);
            if (n > 0) {
                session.body_written += static_cast<std::size_t>(n);
            } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                /* Write error: kill the CGI process */
                kill(session.pid, SIGKILL);
                _finalizeCgi(*client);
                return;
            }
        }

        /* If body write complete, close stdin and move to reading output */
        if (session.body_written >= session.body.size()) {
            close(session.stdin_fd);
            session.stdin_fd = -1;
            _epoll.del(fd);
            _cgiPipeToClient.erase(fd);
            /* Deregister stdin, then arm stdout for reading */
            session.state = CgiSession::STATE_READING_OUTPUT;
            if (!_epoll.mod(session.stdout_fd, EPOLLIN | EPOLLHUP)) {
                std::cerr << "[EventLoop] Failed to rearm stdout_fd for reading\n";
                kill(session.pid, SIGKILL);
                _finalizeCgi(*client);
            }
        }
    }

    /* EPOLLIN: read available data from stdout */
    if ((events & EPOLLIN) && fd == session.stdout_fd && session.state == CgiSession::STATE_READING_OUTPUT) {
        char buf[4096];
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n > 0) {
            if (session.output.size() + static_cast<std::size_t>(n) > session.max_output_bytes) {
                /* Output limit reached: truncate and finish */
                std::size_t allowed = session.max_output_bytes - session.output.size();
                session.output.append(buf, allowed);
                kill(session.pid, SIGKILL);
                _finalizeCgi(*client);
            } else {
                session.output.append(buf, static_cast<std::size_t>(n));
            }
        } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            /* Read error: finalize with what we have */
            _finalizeCgi(*client);
        }
    }

    /* EPOLLHUP: stdout closed by CGI process (EOF) */
    if ((events & EPOLLHUP) && fd == session.stdout_fd) {
        /* Drain any remaining data before finalizing */
        char buf[4096];
        ssize_t n;
        while ((n = read(fd, buf, sizeof(buf))) > 0) {
            if (session.output.size() + static_cast<std::size_t>(n) > session.max_output_bytes) {
                std::size_t allowed = session.max_output_bytes - session.output.size();
                session.output.append(buf, allowed);
                break;
            }
            session.output.append(buf, static_cast<std::size_t>(n));
        }
        _finalizeCgi(*client);
    }
}

/* Finalize CGI session: waitpid, parse output, write response, cleanup.
   Called when stdout is closed (EPOLLHUP) or on error/timeout. */
void EventLoop::_finalizeCgi(Client &client) {
    if (!client.cgi) return;

    CgiSession &session = *client.cgi;

    /* Reap the child process and capture exit code */
    int status = 0;
    pid_t done = waitpid(session.pid, &status, WNOHANG);
    if (done != session.pid) {
        /* Child hasn't exited yet; try once more with blocking (safety net) */
        waitpid(session.pid, &status, 0);
    }

    if (WIFEXITED(status)) {
        session.exit_code = WEXITSTATUS(status);
    } else {
        session.exit_code = 128;
    }

    /* Build HTTP response from CGI output */
    HttpResponse response;
    std::string headerSection, body;

    if (CgiExecutor::parseOutput(session.output, headerSection, body)) {
        /* Parse headers from CGI output */
        std::istringstream iss(headerSection);
        std::string line;
        bool statusSet = false;
        while (std::getline(iss, line)) {
            if (!line.empty() && line[line.size() - 1] == '\r') {
                line.erase(line.size() - 1);
            }
            size_t colonPos = line.find(':');
            if (colonPos != std::string::npos) {
                std::string key = line.substr(0, colonPos);
                std::string val = (colonPos + 1 < line.size()) ? line.substr(colonPos + 1) : "";
                if (!val.empty() && val[0] == ' ') val.erase(0, 1);

                if (key == "Status") {
                    response.setStatus(atoi(val.c_str()));
                    statusSet = true;
                } else if (key != "Content-Length") {
                    response.setHeader(key, val);
                }
            }
        }
        if (!statusSet) {
            response.setStatus(session.exit_code == 0 ? 200 : 500);
        }
        response.setBody(body, "text/html");
    } else {
        /* Failed to parse: treat as plain text or error */
        if (session.exit_code == 0) {
            response.setStatus(200).setBody(session.output, "text/plain");
        } else {
            response.setStatus(500);
        }
    }

    /* Write response into client writeBuf */
    client.writeBuf = response.serialize();

    /* Deregister pipes from epoll */
    _cleanupCgiSession(client);

    /* Flip client socket to EPOLLOUT so response gets written */
    if (!_epoll.mod(client.fd, EPOLLOUT | EPOLLRDHUP)) {
        std::cerr << "[EventLoop] Failed to rearm client fd for writing response\n";
        _conn.closeClient(client.fd);
    }
}

/* Cleanup CGI session: close pipes, remove from epoll tracking, free session. */
void EventLoop::_cleanupCgiSession(Client &client) {
    if (!client.cgi) return;

    CgiSession &session = *client.cgi;

    /* Deregister and close pipes */
    if (session.stdin_fd >= 0) {
        _epoll.del(session.stdin_fd);
        _cgiPipeToClient.erase(session.stdin_fd);
        close(session.stdin_fd);
        session.stdin_fd = -1;
    }
    if (session.stdout_fd >= 0) {
        _epoll.del(session.stdout_fd);
        _cgiPipeToClient.erase(session.stdout_fd);
        close(session.stdout_fd);
        session.stdout_fd = -1;
    }

    /* Delete session object */
    delete client.cgi;
    client.cgi = nullptr;
}
