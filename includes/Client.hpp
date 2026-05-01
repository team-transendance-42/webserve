#pragma once

#include <string>
#include <ctime>
#include "HttpRequest.hpp"

/**
In C++, struct and class are almost identical. The only difference is:
struct members are public by default and class members are private by default.
Both can have constructors, destructors, private/public/protected sections, and member functions. struct can inherit from another struct or class.
Both struct and class can be used as base or derived types.

explicit: without this key word can do:
void process(Client c) {}
process(5); // Implicitly converts 5 (int) to Client(5)

but with explicit:
struct Client { explicit Client(int fd) {} };
void process(Client c) {}
// process(5); // Error! No implicit conversion.
process(Client(5)); // Correct: explicitly create Client object

if not needed implicit conversions, it’s best practice to use explicit.
In programming, “implicit” means something happens automatically, without you writing it directly.
todo: client doesnt init request and writeBuff !!
---------------------- data flow chain (bytes -> text)
the conversion happens layer-by-layer:
1. ssize_t bytes = recv(client.fd, &chunk[0], chunk.size(), 0); // bytes = number of bytes read into buffer
2. Bytes are wrapped in a std::string (ConnectionManager.cpp): std::string chunk
3. Passed to parser (ConnectionManager.cpp:46): ParseResult result = client.request.feed(&chunk[0], static_cast<size_t>(bytes));
4. Parser appends to internal _buf (HttpRequest.hpp): std::string _buf;  // accumulates raw bytes until a complete HTTP request is parsed
5. Parser extracts text fields (when feed() returns COMPLETE)
---
HTTP is text protocol (ASCII headers + binary body).
C++ std::string can store both text and binary data (it's just bytes).
Parser treats _buf as raw byte stream, extracts text line-by-line.
Headers and body remain as strings in Client struct.
---
bytes → string chunk → feed() → buf accumulation → parse → fields extracted → stored in client.request.
*/

/**
 * Represents a single client connection to the server.
 * Holds the socket file descriptor, parsed HTTP request, response buffer, and keep-alive state.
 * Used by ConnectionManager to track and manage each active client.
 */
struct Client {
    int         fd;
    HttpRequest request;
    std::string writeBuf;
    bool        keep_alive;
    // int         bytesSent; // for tracking partial writes   todo !!
    // int         bytesToSend; // for tracking total response size todo !!
    // int         bytesWritten; // for tracking how many bytes have been written so far todo !!
    // int         bytesLeft; // for tracking how many bytes are left to write todo !!
    time_t      lastTimestamp; // for tracking last activity time (for timeouts): init to curr time; time_t: depend on system, usually a typedef for a numeric type representing seconds since epoch (Jan 1, 1970). time() returns current time as time_t.(often long or long long)

    explicit Client(int fd) : fd(fd), keep_alive(false), lastTimestamp(std::time(0)) {}

    Client(const Client &) = delete;
    Client &operator=(const Client &) = delete;
};