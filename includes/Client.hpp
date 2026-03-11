// includes/Client.hpp
#pragma once
#include "HttpRequest.hpp"
#include <string>

/**
In C++, struct and class are almost identical. The only difference is:
struct members are public by default.
class members are private by default.
Both can have constructors, destructors, private/public/protected sections, and member functions.
struct can inherit from another struct or class.
Both struct and class can be used as base or derived types.
*/
struct Client {
    int         fd;
    HttpRequest request;
    std::string write_buf;
    bool        keep_alive;

    explicit Client(int fd) : fd(fd), keep_alive(false) {}

    Client(const Client &) = delete;
    Client &operator=(const Client &) = delete;
};