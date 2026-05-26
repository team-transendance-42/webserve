#pragma once

#include <string>
#include <ctime>
#include "HttpRequest.hpp"

/**
In C++, struct and class are almost identical. The only difference is:
struct members are public by default and class members are private by default.

explicit:
// process(5); // Error! No implicit conversion.
process(Client(5)); // Correct: explicitly create Client object
---
HTTP is text protocol (ASCII headers + binary body).
std::string can store both text and binary data (it's just bytes).
Parser treats _buf as raw byte stream, extracts text ine-by-line. Headers and body remain as strings in Client struct.
---
bytes → string chunk → feed() → buf accumulation → parse → fields extracted → stored in client.request.
*/

/**
 * Represents a single client connection to the server.
 * Holds the socket file descriptor, parsed HTTP request, response buffer, and keep-alive state.
 * Used by ConnectionManager to track and manage each active client.
 * struct: by default fields are public
 */
struct Client {
    int         fd;
    HttpRequest request;
    std::string writeBuf;
    bool        keep_alive;
    time_t      lastTimestamp;

    explicit Client(int fd) : fd(fd), keep_alive(false), lastTimestamp(std::time(0)) {}

    Client(const Client &) = delete;
    Client &operator=(const Client &) = delete;
};