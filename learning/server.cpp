#include <iostream>
#include <cstring> // strlen
#include <unistd.h> // close(server_fd)
#include <arpa/inet.h> // sockeadrr_in, htons
#include <netinet/in.h>


/**
for low-level networking, the POSIX socket API is still standard and there’s no direct C++ replacement in the standard library.

socket(AF_INET, SOCK_STREAM, 0) Creates a new socket (communication endpoint).
AF_INET: Use IPv4 addresses.
SOCK_STREAM: Use TCP (reliable, connection-based).
0: Default protocol (TCP).

address.sin_addr.s_addr = INADDR_ANY;
Sets the IP address to INADDR_ANY.
Means “listen on all available network interfaces” (any IP address assigned to the machine).
address.sin_port = htons(8080);
Sets the port number to 8080.
htons converts the port from host byte order to network byte order (required for network communication).

recv(fd, buffer, size, flags)
Receives bytes.
Returns: number of bytes, 0 if connection closed, -1 on error. Blocking by default.
*/
// c++ server.cpp
// after that you can test it with curl or a web browser by going to http://localhost:8080

int main()
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0)
    {
        std::cerr << "Socket creation failed\n";
        return 1;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080); // Host To Network Short.

    if (bind(server_fd, (sockaddr*)&address, sizeof(address)) < 0)
    {
        std::cerr << "Bind failed\n";
        return 1;
    }

    if (listen(server_fd, 10) < 0)
    {
        std::cerr << "Listen failed\n";
        return 1;
    }

    std::cout << "Server running on port 8080\n";

    while (true)
    {
        int client_fd = accept(server_fd, nullptr, nullptr);

        if (client_fd < 0)
            continue;

        char buffer[4096] = {0};

        ssize_t bytes = recv(client_fd, buffer, sizeof(buffer), 0);

        if (bytes > 0)
        {
            std::cout << "Request:\n" << buffer << "\n";

            const std::string body = "Hello";
            const std::string response =
                "HTTP/1.1 200 OK\r\n"
                "Content-Length: " + std::to_string(body.size()) + "\r\n"
                "Content-Type: text/plain\r\n"
                "Connection: close\r\n"
                "\r\n" + body;

            send(client_fd, response.c_str(), response.size(), 0);
        }

        close(client_fd);
    }

    close(server_fd);
}