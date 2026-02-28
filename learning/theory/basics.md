**What is a socket**

A socket is:
An endpoint for communication between two programs.
Think of it like:

Program A  ← socket →  Network  ← socket →  Program B
It's just a file descriptor created by the OS that allows communication.

On Linux everything is a file:
files, pipes, sockets,terminals
Sockets are just special file descriptors.

socket() system call
Creates communication endpoint.
int socket(int domain, int type, int protocol);

Example:
int server_fd = socket(AF_INET, SOCK_STREAM, 0);
AF_INET → IPv4
SOCK_STREAM → TCP
0 → default protocol
--------------------------

**What is TCP**

TCP = Transmission Control Protocol.
It is:
Reliable, Ordered, Error-checked, Connection-based

When you open a website:
Browser ↔ TCP ↔ Server
TCP guarantees:
No data loss, Correct order, Automatic retransmission
Webserve uses TCP.
----------------------------------------------

**File descriptor*

An integer that represents an open resource.
0 → stdin
1 → stdout
2 → stderr
3 → socket
4 → another socket
int server_fd = socket(...);
--------------------------------

**bind()**

connects your socket to:
IP address, Port

Example:
bind(server_fd, 
, sizeof(address));

Without bind, OS assigns random port.

For server, you MUST bind.
----------------------------------

**listen()**

Tells OS: This socket will accept incoming connections.

Example:
listen(server_fd, 10);
10 = backlog (queue size)
-----------------------------------

**accept()**

When a client connects:
int client_fd = accept(server_fd, ...);

Important:
server_fd → listening socket
client_fd → new socket for communication
Now you communicate with client_fd.
-----------------------------------

**recv() / send()**

Receive data:
char buffer[1024];
recv(client_fd, buffer, 1024, 0);

Send data:
send(client_fd, "Hello", 5, 0);
That's how HTTP response is sent.
------------------------------------

Blocking vs non-blocking

Blocking
If no client connects:
accept() → waits forever
Program freezes.

Non-blocking
If no client connects:
accept() → returns immediately
Used for multiple clients.

You enable with:
fcntl(fd, F_SETFL, O_NONBLOCK);
stands for "file control." It is a system call in Unix/Linux used to manipulate file descriptors.
*F_SETFL tells fcntl to set file status flags.
*O_NONBLOCK makes the file descriptor non-blocking: operations like accept(), recv(), or read() return immediately if no data is available, instead of waiting.
------------------------------------

poll() / select() / epoll()

Used to handle MANY clients without threads.

poll()
Waits until:
socket ready to read
socket ready to write
if socket ready → read
if new connection → accept

Bigger projects use epoll() → more efficient for many clients.
-------------------------------

Big picture:

Normal server:
socket()
bind()
listen()
while (true)
{
    accept()
    recv()
    send()
}

Advanced server:
socket()
bind()
listen()
set non-blocking

while (true)
{
    poll()
    accept()
    recv()
    send()
}


