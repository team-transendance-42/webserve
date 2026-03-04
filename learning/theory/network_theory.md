Synopsis – Beej’s Guide (Sections 2–3)

1. Two Types of Internet Sockets

Stream sockets (SOCK_STREAM)

Reliable, ordered, error-free communication.

Use TCP (Transmission Control Protocol).

Examples: telnet, HTTP (web browsers).

Data arrives in the same order it was sent.

Datagram sockets (SOCK_DGRAM)

Connectionless, unreliable, unordered.

Use UDP (User Datagram Protocol).

Packets may be lost or arrive out of order.

Applications implement their own reliability (e.g., TFTP uses ACKs).

2. Data Encapsulation

When sending data:
Application data → wrapped in protocol header (e.g., TFTP) → wrapped in UDP → wrapped in IP → wrapped in Ethernet → sent.

On receive:
Each layer strips its header until the application gets the original data.

Layered models:

Full OSI: Application → Presentation → Session → Transport → Network → Data Link → Physical

Practical Internet model: Application → Transport (TCP/UDP) → Internet (IP) → Network Access (Ethernet)

Sockets programmers only deal with Application + Transport level; kernel handles lower layers.

3. Key C Structures

Socket descriptor → int

struct sockaddr

Generic socket address

Contains sa_family and raw address data

struct sockaddr_in

Used for IPv4

sin_family → must be AF_INET

sin_port → port (Network Byte Order)

sin_addr.s_addr → IP (Network Byte Order)

sin_zero → padding (set to zero)

You cast struct sockaddr_in* to struct sockaddr* when calling socket functions.

4. Byte Order (Very Important)

Two formats:

Host Byte Order

Network Byte Order (Big Endian)

Conversion functions:

htons() → Host to Network (short)

htonl() → Host to Network (long)

ntohs() → Network to Host (short)

ntohl() → Network to Host (long)

Use these for sin_port and sin_addr.

5. IP Address Functions

Convert string → binary:

inet_addr() (older, less safe)

inet_aton() (preferred, safer)

Convert binary → string:

inet_ntoa()

Returns pointer to static buffer (gets overwritten on next call)

Core Idea of This Section

Understand difference between TCP and UDP.

Understand how packets are layered and encapsulated.

Learn how to build and fill sockaddr_in.

Learn to handle Network Byte Order.

Learn basic IP string ↔ binary conversion.

This is the theoretical foundation before writing real socket programs.