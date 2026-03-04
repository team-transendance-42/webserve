Network Layering and Encapsulation

When data is sent over a network, it is wrapped in multiple protocol headers. Each layer adds its own header before transmission. On the receiving side, each layer removes its corresponding header until the original data remains. This process is called data encapsulation.

Typical layers (from highest to lowest):

Application

Transport (Transmission Control Protocol or User Datagram Protocol)

Internet (Internet Protocol)

Network Access (Ethernet or other hardware)

Physical (hardware transmission)

Socket programmers interact mainly with the application and transport layers. The operating system kernel builds the lower-level headers automatically.

Socket Programming Basics

A socket descriptor is simply an int.

Network communication uses two byte orders:

Host Byte Order (machine’s internal format)

Network Byte Order (big-endian format used on the network)

Before sending numerical values such as port numbers or IP addresses, they must be converted to Network Byte Order.

Conversion functions:

htons() – Host to Network Short

htonl() – Host to Network Long

ntohs() – Network to Host Short

ntohl() – Network to Host Long

Port numbers and IP addresses must be in Network Byte Order. The address family field remains in Host Byte Order because it is not transmitted over the network.

Address Structures

struct sockaddr is a generic structure for socket addresses.

struct sockaddr_in is used for Internet Protocol version 4 addresses and contains:

Address family

Port number

IP address

Padding bytes

A struct sockaddr_in can be cast to struct sockaddr when calling socket functions.

The IP address is stored inside sin_addr.s_addr in Network Byte Order.

IP Address Conversion

To convert a string IP address (for example, "10.12.110.57") to binary form:

inet_addr() converts text to Network Byte Order (returns -1 on error).

inet_aton() converts text to binary and stores it in a structure (returns non-zero on success).

To convert a binary IP address back to text:

inet_ntoa() converts a binary address to dotted-decimal string form.

It returns a pointer to static memory, so the result should be copied if it needs to be stored.

In short:
Data travels through layered protocols using encapsulation. Socket programming requires correct address structures and proper byte order conversion. The operating system handles most low-level details, but the programmer must correctly prepare addresses and convert values to Network Byte Order.
--------------------------------------------------

