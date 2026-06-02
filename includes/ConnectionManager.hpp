#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include "Client.hpp"
#include "Listener.hpp"

/**
Switches client connection between reading and writing states, and handles closing connections on errors or disconnects.
ProcessRequest is selected per-client via the clientToListener routing table
(owned by EventLoop, passed in by reference) — one Listener owns the
ProcessRequest for its (host, port) virtual-host group.
*/
class ConnectionManager {
public:
    ConnectionManager(std::map<int, Client *> &clients,
                      std::map<int, Listener *> &clientToListener,
                      std::function<void(int, uint32_t)> epollMod,
                      std::function<void(int)> epollDel,
                      std::function<void(Client&)> registerCgiPipes,
                      std::function<void(Client&)> cleanupCgi);

    void readClient(Client &client, std::size_t readBufSize);
    void writeClient(Client &client);
    void closeClient(int fd);

private:
    std::map<int, Client *>             &_clients;
    std::map<int, Listener *>           &_clientToListener;
    /* the second arg uint32_t: (EPOLLOUT | EPOLLRDHUP) is an integer value (specifically, a bitmask of event flags) */
    std::function<void(int, uint32_t)>  _epollMod;
    std::function<void(int)>            _epollDel;
    std::function<void(Client&)>        _registerCgiPipes;
    std::function<void(Client&)>        _cleanupCgi;
};
