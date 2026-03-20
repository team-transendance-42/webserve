#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>

#include "Client.hpp"

class ConnectionManager {
public:
    ConnectionManager(std::map<int, Client *> &clients,
                      std::function<void(int, uint32_t)> epoll_mod,
                      std::function<void(int)> epoll_del,
                      std::function<void(Client &)> process_request);

    void readClient(Client &client, std::size_t read_buf_size);
    void writeClient(Client &client);
    void closeClient(int fd);

private:
    std::map<int, Client *> &_clients;
    std::function<void(int, uint32_t)> _epollMod;
    std::function<void(int)> _epollDel;
    std::function<void(Client &)> _processRequest;
};
