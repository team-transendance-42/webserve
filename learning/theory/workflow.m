Start webserv
     ↓
Read + parse configuration file
     ↓
Create one or more listening sockets (ports)
     ↓
Enter main event loop (poll/select/epoll)
     ↓
For each event:
    - accept new clients
    - read incoming HTTP requests
    - parse the request
    - generate a response (file, CGI, error, etc.)
    - write the response back
     ↓
Repeat forever (without blocking)