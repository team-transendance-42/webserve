int flags = fcntl(fd, F_GETFL, 0); // from main.cpp

a system call in C/C++ used to get the file status flags for a file descriptor (fd).
fcntl stands for "file control". It's a POSIX function used to manipulate file descriptors.
fd is the file descriptor you want to query or modify (e.g., a socket or file).
F_GETFL is a command that tells fcntl to "get file status flags" (like O_NONBLOCK, O_APPEND, etc.).
The third argument (0 here) is ignored for F_GETFL.

This call retrieves the current flags set on fd (such as whether it's non-blocking).
The result (flags) is an integer bitmask. You can check or modify these flags.

static void setNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) { perror("fcntl F_GETFL"); return; }
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

Normally, operations like read() or write() on a socket or file will "block" (pause) your program until data is available or the operation completes.
In non-blocking mode, these operations return immediately, even if they can’t complete right away. This is essential for event-driven servers (like your web server) that handle many connections at once.
fcntl(fd, F_SETFL, flags | O_NONBLOCK);
Sets the O_NONBLOCK flag, making fd non-blocking. It keeps the old flags and adds O_NONBLOCK.
For servers using poll(), select(), or epoll(), non-blocking sockets let you handle many clients efficiently without getting stuck waiting for one.
-----------------------
This function makes a socket or file descriptor non-blocking, so our web server can handle multiple connections without waiting on any single one. This is a standard technique in network programming.
------------------------
fcntl(fd, F_SETFL, flags | O_NONBLOCK);

the | symbol is the bitwise OR operator in C/C++. Here’s what it does in detail:

flags is an integer where each bit represents a different file status flag (like O_APPEND, O_NONBLOCK, etc.).
O_NONBLOCK is a constant with a specific bit set, representing the "non-blocking" mode.
When you write flags | O_NONBLOCK, you combine the existing flags with the O_NONBLOCK flag:

If O_NONBLOCK is already set in flags, nothing changes.
If it’s not set, this operation turns on just the O_NONBLOCK bit, leaving all other bits unchanged.
This way, you keep all the previous settings (like append mode, etc.) and add non-blocking mode.
------------------------
------------------------
socket(AF_INET, SOCK_STREAM, 0);

AF_INET: Address Family INET. This means the socket will use IPv4 addresses (like 127.0.0.1 or 192.168.1.1).
SOCK_STREAM: This specifies the socket type. SOCK_STREAM means a stream socket, which provides reliable, connection-oriented communication (TCP).
0: Protocol. 0 lets the system choose the default protocol for the given type (for AF_INET + SOCK_STREAM, this is TCP).

This line creates a TCP socket for IPv4 networking. It’s the first step in setting up a server or client for network communication.
-------------------------
setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

setsockopt is a system call used to set options on a socket. Here’s what each argument means:
fd: The file descriptor of the socket you want to configure.
SOL_SOCKET: This indicates that the option is a socket-level option (as opposed to, say, a TCP-level option).
SO_REUSEADDR: This is the specific option being set. It allows the socket to bind to an address that is already in use. This is useful for servers that restart frequently, as it allows them to reuse the same port without waiting for the OS to release it.
&opt: A pointer to the value of the option. In this case, opt is typically

-----------------------------
from main.cpp
 addr.sin_port        = htons(static_cast<uint16_t>(cfg.port));

Stands for "host to network short".
It converts a 16-bit number (short) from the host's byte order to network byte order (big-endian), which is required for network protocols.
-------------------------------

listen(fd, 128); (in main.cpp) puts the socket (fd) into listening mode, making it ready to accept incoming connection requests.

fd: The socket file descriptor.
128: The backlog, which is the maximum number of pending connections that can be queued up before the kernel starts rejecting new ones.
In summary:
This line tells the operating system that your socket should accept incoming connections, and it can queue up to 128 pending connections before refusing new ones. This is a key step in setting up a TCP server.
------------------------------
------------------------------

char buf[8192]; creates a buffer of 8192 bytes (8 KB) for reading data from a socket.

8192 bytes (8 KB) is a common buffer size in network programming.
It’s large enough to efficiently handle most HTTP requests and responses in a single read, reducing the number of system calls.
It matches or is close to typical system page sizes (often 4 KB or 8 KB), which can improve performance.
It’s not too large, so it doesn’t waste memory for each client.
In summary:
8192 is a practical, efficient default size for network I/O buffers—big enough for most web requests, but not excessive.
------------------------------
------------------------------

The explicit keyword in C++ is used before a constructor (or conversion operator) to prevent implicit conversions.

Synonym:
"only allow direct/intentional use"
"no automatic conversion"

explicit means the constructor can only be called directly, not by automatic type conversion. It helps prevent bugs from unintended conversions.
-----------------------------

If you want to save logs to a file:
Start your server and redirect output:
./webserv > server.log 2>&1
then view logs with:
tail -f server.log or less server.log
------------------------------
------------------------------
poll() is a system call used for monitoring multiple file descriptors to see if they have any events (like incoming data, errors, etc.). It's an alternative to select() and is more scalable for large numbers of connections.

poll() takes an array of pollfd structures, each representing a file descriptor and the events you want to monitor. It blocks until one or more events occur, allowing you to efficiently manage multiple clients without busy-waiting.

poll() is commonly used in network servers to handle multiple client connections simultaneously. It allows the server to react to events on any of the monitored file descriptors without blocking on any single one.

struct pollfd {
    int   fd;      // The file descriptor to monitor (socket, file, etc.)
    short events;  // Events to watch for (e.g., POLLIN for read, POLLOUT for write)
    short revents; // Events that occurred (set by poll())
};

pollfd is a struct used with the poll() system call in Unix/Linux for I/O multiplexing.
--------------------------------------

poll() is a system call used for I/O multiplexing. It lets you monitor multiple file descriptors (sockets, files, etc.) to see if they are ready for reading, writing, or have errors.
It’s commonly used in network servers to efficiently handle many clients without blocking.

int ready = poll(fds.data(), static_cast<nfds_t>(fds.size()), 3000); // returns number of fds with events or 0 on timeout, -1 if an error occurs

fds.data()

fds is a std::vector<pollfd>. Each pollfd struct represents a file descriptor and the events you want to monitor.
fds.data() gives a pointer to the first element of the array, as required by poll().
static_cast<nfds_t>(fds.size())

nfds_t is the type poll() expects for the number of file descriptors.
fds.size() is the number of file descriptors you’re monitoring.
static_cast converts the size to the correct type.
3000

This is the timeout in milliseconds (ms).
Here, poll() will wait up to 3000 ms (3 seconds) for any file descriptor to become ready.
If no events occur in that time, poll() returns 0 (timeout).

--------------------------------------
nfds_t stands for "number of file descriptors type."
It is an unsigned integer type used by the poll() system call to specify how many file descriptors you are passing in (the size of the pollfd array).
It ensures the value is the correct type for the system call, typically defined as typedef unsigned long nfds_t; on Linux.
--------------------------------------

int clientFd = accept(listenFd, reinterpret_cast<sockaddr*>(&ca), &len);

accept() is a system call used on a listening socket (listenFd) to accept a new incoming connection.
When a client tries to connect, accept() creates a new socket (clientFd) for communicating with that client.

listenFd: The file descriptor of the listening socket (created with socket(), bound with bind(), and set to listen() mode).
This socket waits for incoming connections.

reinterpret_cast<sockaddr*>(&ca): ca is a sockaddr_in struct, which will hold the client’s address (IP and port).
accept() expects a pointer to a generic sockaddr struct, so you cast ca to sockaddr*.
After accept(), ca will contain the client’s address info.

&len: en is a variable of type socklen_t, set to sizeof(ca).
accept() will update len to the actual size of the address returned.


























































