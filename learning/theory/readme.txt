Your program must not crash under any circumstances (even if it runs out of memory) or terminate unexpectedly

Program Name webserv
Files to Submit Makefile, *.{h, hpp}, *.cpp, *.tpp, *.ipp, configuration files

Makefile NAME, all, clean, fclean, re

Arguments [A configuration file]

External Function
execve, pipe, strerror, gai_strerror, errno, dup,
dup2, fork, socketpair, htons, htonl, ntohs, ntohl,
select, poll, epoll (epoll_create, epoll_ctl,
epoll_wait), kqueue (kqueue, kevent), socket,
accept, listen, send, recv, chdir, bind, connect,
getaddrinfo, freeaddrinfo, setsockopt, getsockname,
getprotobyname, fcntl, close, read, write, waitpid,
kill, signal, access, stat, open, opendir, readdir
and closedir.

Even though poll() is mentioned in the subject and evaluation sheet,
you can use any equivalent function such as select(), kqueue(), or
epoll().

Please read the RFCs defining the HTTP protocol, and perform tests
with telnet and NGINX before starting this project.
Although you are not required to implement the entire RFCs, reading
it will help you develop the required features.
The HTTP 1.0 is suggested as a reference point, but not enforced.
