struct sockaddr_in address;
bind(server_fd, (struct sockaddr*)&address, sizeof(address));


&address: Gets the memory address of the variable address. The type is struct sockaddr_in* if address is struct sockaddr_in.
(struct sockaddr*): This is a type cast. It tells the compiler to treat the address as a pointer to struct sockaddr, not struct sockaddr_in.
Why do this?

Many socket functions (like bind, connect) expect a struct sockaddr* argument, but you usually have a struct sockaddr_in variable.
So, you take the address of your struct sockaddr_in (&address), then cast it to struct sockaddr*.
------------------------------

struct sockaddr
Generic address structure for sockets.
Used by socket functions (bind, connect, accept).
Contains minimal fields: family and data.
struct sockaddr {
    unsigned short sa_family; // Address family (AF_INET, etc.)
    char sa_data[14];         // Protocol-specific address info
};

struct sockaddr_in
Specific to IPv4 addresses.
Contains more fields: port, IP address, etc.

struct sockaddr_in {
    short sin_family;         // Address family (AF_INET)
    unsigned short sin_port;  // Port number (network byte order)
    struct in_addr sin_addr;  // IP address
    char sin_zero[8];         // Padding
};
-------------------------------
Why cast sockaddr_in* to sockaddr*?
Functions like bind, connect, accept expect struct sockaddr*.
You usually have struct sockaddr_in for IPv4.
Cast is needed to match function signature.

struct sockaddr_in address;
address.sin_family = AF_INET;
address.sin_port = htons(8080);
address.sin_addr.s_addr = INADDR_ANY;

bind(server_fd, (struct sockaddr*)&address, sizeof(address));
------------------------------
struct sockaddr*: Generic pointer for socket address.
struct sockaddr_in*: Pointer for IPv4 address.
Cast sockaddr_in* to sockaddr* when calling socket functions.


