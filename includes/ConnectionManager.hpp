#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include "Client.hpp"

class ProcessRequest;

/**
Switches client connection between reading and writing states, and handles closing connections on errors or disconnects.
*/
class ConnectionManager {
public:
    ConnectionManager(std::map<int, Client *> &clients,
                      std::function<void(int, uint32_t)> epollMod,
                      std::function<void(int)> epollDel,
                      ProcessRequest &processorReq);

    void readClient(Client &client, std::size_t readBufSize);
    void writeClient(Client &client);
    void closeClient(int fd);

private:
    std::map<int, Client *>             &_clients;
    /* the second arg (EPOLLOUT | EPOLLRDHUP) is an integer value (specifically, a bitmask of event flags) */
    std::function<void(int, uint32_t)>  _epollMod;
    std::function<void(int)>            _epollDel;
    ProcessRequest                      &_processorRequest;
};
