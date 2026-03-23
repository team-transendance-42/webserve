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
todo: client doesnt init request and writeBuff !!
---------------------- data flow chain (bytes -> text)
the conversion happens layer-by-layer:
1. ssize_t bytes = recv(client.fd, &chunk[0], chunk.size(), 0); // bytes = number of bytes read into buffer

2. Bytes are wrapped in a std::string (ConnectionManager.cpp): std::string chunk(read_buf_size, '\0'); // empty string buffer
recv(..., &chunk[0], chunk.size(), 0); // receive into string's char array

3. Passed to parser (ConnectionManager.cpp:46): ParseResult result = client.request.feed(&chunk[0], static_cast<size_t>(bytes));
// feed() takes char* and length

4. Parser appends to internal _buf (HttpRequest.hpp): std::string _buf;  // accumulates raw bytes until a complete HTTP request is parsed

5. Parser extracts text fields (when feed() returns COMPLETE): // From _buf, parser fills these:
std::string method;         // "GET", "POST", etc.
std::string path;           // "/index.html"
std::map<std::string, std::string> headers;  // "Content-Type": "application/json"
std::string body;           // request body as text
---
HTTP is text protocol (ASCII headers + binary body).
C++ std::string can store both text and binary data (it's just bytes).
Parser treats _buf as raw byte stream, extracts text line-by-line.
Headers and body remain as strings in Client struct.
---
bytes → string chunk → feed() → buf accumulation → parse → fields extracted → stored in client.request.
*/
struct Client {
    int         fd;
    HttpRequest request;
    std::string writeBuf;
    bool        keep_alive;

    explicit Client(int fd) : fd(fd), keep_alive(false) {}

    Client(const Client &) = delete;
    Client &operator=(const Client &) = delete;
};