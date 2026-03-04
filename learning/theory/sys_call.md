System Calls for Network Programming

Network communication in Unix is done through system calls. The typical order of calls depends on whether you are creating a client or a server.

1. socket()

Creates a socket and returns a socket descriptor (an integer).

Arguments:

Domain: usually AF_INET (Internet Protocol version 4)

Type: SOCK_STREAM (Transmission Control Protocol) or SOCK_DGRAM (User Datagram Protocol)

Protocol: usually 0 (automatic selection)

Returns:

Socket descriptor on success

-1 on error

This only creates the socket; it does not connect or listen yet.

2. bind()

Associates a socket with a local IP address and port number.

Required mainly for servers that wait for incoming connections.

Important points:

Port and IP address must be in Network Byte Order.

Ports below 1024 are reserved.

INADDR_ANY allows the system to use the machine’s IP automatically.

Port 0 lets the system choose an available port.

Returns -1 on error.

setsockopt() with SO_REUSEADDR can prevent “Address already in use” errors.

3. connect()

Used by clients to connect to a remote host.

Arguments:

Socket descriptor

Destination address (IP and port)

Size of address structure

Important:

No need to call bind() for clients in most cases.

The system automatically assigns a local port.

Returns -1 on error.

4. listen()

Used by servers after bind().

Marks the socket as passive, meaning it waits for incoming connections.

Arguments:

Socket descriptor

Backlog (maximum number of queued pending connections)

Returns -1 on error.

5. accept()

Used by servers to accept an incoming connection.

Behavior:

Takes a connection from the queue.

Returns a new socket descriptor for communication with that client.

The original socket continues listening for new connections.

Important:

Use the new socket descriptor for sending and receiving data.

Returns -1 on error.

Final server sequence:

socket()
bind()
listen()
accept()

------------------------------------------------------------------
Summary

Client flow:

socket()
connect()

Server flow:

socket()
bind()
listen()
accept()

socket() creates the endpoint.
bind() assigns a local port.
connect() connects to a remote host.
listen() prepares to receive connections.

























