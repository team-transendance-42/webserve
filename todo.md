tests:
telnet, netcat, nc, curl req

very long body but allowed what happens
piping cgi

pathtraversal
execute files with not allowed permissions(delete, post etc)

=============
Telnet
--------------
 is a network protocol and command-line tool that lets you connect to remote computers over TCP/IP. It opens a raw text-based connection to a specified host and port, allowing you to manually send and receive data. It’s often used for testing servers, debugging network services, or interacting with devices that accept plain text commands.
Opens a raw TCP connection to a host:port.
Lets you type and send data manually.
Good for testing text-based protocols (HTTP, SMTP, etc.).
---------
nc (netcat)
------------
What it does:

Opens TCP or UDP connections, listens for connections, or sends data.
More flexible than telnet (can send files, listen, use UDP, etc.).
Good for scripting, port scanning, and raw data transfer.
nc 127.0.0.1 8080

Type your request and press Enter twice (for HTTP).
To send a file:
nc 127.0.0.1 8080 < myfile.txt

To listen for connections
nc -l 1234
----------
curl
--------------
What it does:

Command-line tool for transferring data with URLs.
Supports HTTP, HTTPS, FTP, and many more protocols.
Handles headers, cookies, authentication, file uploads, etc.
Not interactive—sends requests and prints responses.
curl http://127.0.0.1:8080/

curl -X POST -d "hello" http://127.0.0.1:8080/api

add headers:
curl -H "X-Test: 1" http://127.0.0.1:8080/

set output to file:
curl -o output.html http://127.0.0.1:8080/

===================
telnet 127.0.0.1 8080
enter
GET / HTTP/1.1
Host: 127.0.0.1
(Press Enter twice after the last header to send the request.)
==========
POST /api HTTP/1.1
Host: 127.0.0.1
Content-Length: 5

hello
===============
