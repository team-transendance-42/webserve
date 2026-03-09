Introduction
The Hypertext Transfer Protocol (HTTP) is an application protocol for distributed,
collaborative, hypermedia information systems.
HTTP is the foundation of data communication for the World Wide Web, where hypertext documents include hyperlinks to other resources that the user can easily access. For
example, by clicking a mouse button or tapping the screen on a web browser.
HTTP was developed to support hypertext functionality and the growth of the World
Wide Web.
--------------------------------------------------------------------------------------------
The primary function of a web server is to store, process, and deliver web pages to clients.
--------------------------------------------------------------------------------------------
Client-server communication occurs through the Hypertext Transfer Protocol (HTTP).
Pages delivered are most frequently HTML documents, which may include images, style
sheets, and scripts in addition to the text content.
Multiple web servers may be used for a high-traffic website, splitting traffic between multiple physical machines.
A user agent, commonly a web browser or web crawler, initiates communication by requesting a specific resource using HTTP, and the server responds with the content of that
resource or an error message if unable to do so. The resource is typically a real file on
the server’s storage, or the result of a program. But this is not always the case and can
actually be many other things.
Although its primary function is to serve content, HTTP also enables clients to send
data. This feature is used for submitting web forms, including the uploading of files.
-----------------------------

Your program must not crash under any circumstances (even if it runs out of memory) or terminate unexpectedly.
If this occurs, your project will be considered non-functional and your grade will
be 0.
• You must submit a Makefile that compiles your source files. It must not perform
unnecessary relinking.
• Your Makefile must at least contain the rules:
$(NAME), all, clean, fclean and re.
• Compile your code with c++ and the flags -Wall -Wextra -Werror

 Make sure to leverage as many C++ features as possible (e.g., choose <cstring>
over <string.h>). You are allowed to use C functions, but always prefer their C++
versions if possible.
• Any external library and Boost libraries are forbidden.
-------------------------------

Program Name webserv
Files to Submit Makefile, *.{h, hpp}, *.cpp, *.tpp, *.ipp,
configuration files
Makefile NAME, all, clean, fclean, re
Arguments [A configuration file]
External Function All functionality must be implemented in C++ 98.
execve, pipe, strerror, gai_strerror, errno, dup,
dup2, fork, socketpair, htons, htonl, ntohs, ntohl,
select, poll, epoll (epoll_create, epoll_ctl,
epoll_wait), kqueue (kqueue, kevent), socket,
accept, listen, send, recv, chdir, bind, connect,
getaddrinfo, freeaddrinfo, setsockopt, getsockname,
getprotobyname, fcntl, close, read, write, waitpid,
kill, signal, access, stat, open, opendir, readdir
and closedir.
Libft authorized n/a
Description An HTTP server in C++

Your executable should be executed as follows:
./webserv [configuration file]
Even though poll() is mentioned in the subject and evaluation sheet,
you can use any equivalent function such as select(), kqueue(), or
epoll().

Please read the RFCs defining the HTTP protocol, and perform tests
with telnet and NGINX before starting this project.
Although you are not required to implement the entire RFCs, reading
it will help you develop the required features.
The HTTP 1.0 is suggested as a reference point, but not enforced.
----------------------------------------------------
Requirements
• Your program must use a configuration file, provided as an argument on the command line, or available in a default path.
• You cannot execve another web server.
• Your server must remain non-blocking at all times and properly handle client disconnections when necessary.
• It must be non-blocking and use only 1 poll() (or equivalent) for all the I/O
operations between the clients and the server (listen included).
• poll() (or equivalent) must monitor both reading and writing simultaneously.
• You must never do a read or a write operation without going through poll() (or
equivalent).
• Checking the value of errno to adjust the server behaviour is strictly forbidden
after performing a read or write operation.
• You are not required to use poll() (or an equivalent function) for regular disk files;
read() and write() on them do not require readiness notifications.

I/O that can wait for data (sockets, pipes/FIFOs, etc.) must be
non-blocking and driven by a single poll() (or equivalent). Calling
read/recv or write/send on these descriptors without prior readiness
will result in a grade of 0. Regular disk files are exempt.

When using poll() or any equivalent call, you can use every associated macro or
helper function (e.g., FD_SET for select()).
• A request to your server should never hang indefinitely.
• Your server must be compatible with standard web browsers of your choice.
• NGINX may be used to compare headers and answer behaviours (pay attention to
differences between HTTP versions).
• Your HTTP response status codes must be accurate.
• Your server must have default error pages if none are provided.
• You can’t use fork for anything other than CGI (like PHP, or Python, and so forth).
• You must be able to serve a fully static website.
• Clients must be able to upload files.
• You need at least the GET, POST, and DELETE methods

Stress test your server to ensure it remains available at all times.
• Your server must be able to listen to multiple ports to deliver different content (see
Configuration file).
We deliberately chose to offer only a subset of the HTTP RFC. In this
context, the virtual host feature is considered out of scope. But
you are allowed to implement it if you want.
-------------------------------------------------------

**Configuration file**
You can take inspiration from the ’server’ section of the NGINX
configuration file.
In the configuration file, you should be able to:
• Define all the interface:port pairs on which your server will listen to (defining multiple websites served by your program).
• Set up default error pages.
• Set the maximum allowed size for client request bodies.
• Specify rules or configurations on a URL/route (no regex required here), for a
website, among the following:
◦ List of accepted HTTP methods for the route.
◦ HTTP redirection.
◦ Directory where the requested file should be located (e.g., if URL /kapouet
is rooted to /tmp/www, URL /kapouet/pouic/toto/pouet will search for
/tmp/www/pouic/toto/pouet).
◦ Enabling or disabling directory listing.
◦ Default file to serve when the requested resource is a directory.
◦ Uploading files from the clients to the server is authorized, and storage location
is provided.