A socket is a communication endpoint used by programs to send and receive data over a network.

There are two main Internet socket types:

Stream sockets (SOCK_STREAM):
Provide reliable, ordered, two-way communication. Data arrives in the same order it was sent and without errors. They use the Transmission Control Protocol (TCP).

Datagram sockets (SOCK_DGRAM):
Provide connectionless communication. Data is sent in packets that may arrive, arrive out of order, or be lost. If a packet arrives, its data is error-free. They use the User Datagram Protocol (UDP).