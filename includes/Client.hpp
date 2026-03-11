// includes/Client.hpp
#pragma once
#include "HttpRequest.hpp"
#include <string>

struct Client {
    int         fd;
    HttpRequest request;
    std::string write_buf;
    bool        keep_alive;

    explicit Client(int fd) : fd(fd), keep_alive(false) {}

private:
    Client(const Client &);
    Client &operator=(const Client &);
};