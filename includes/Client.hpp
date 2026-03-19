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

explicit: without this key word can do:
struct Client {
    Client(int fd) { }
};
void process(Client c) {}
process(5); // Implicitly converts 5 (int) to Client(5)

but with explicit:
struct Client {
    explicit Client(int fd) {}
};

void process(Client c) {}
// process(5); // Error! No implicit conversion.
process(Client(5)); // Correct: explicitly create Client object

if not needed implicit conversions, it’s best practice to use explicit.
In programming, “implicit” means something happens automatically, without you writing it directly. 
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