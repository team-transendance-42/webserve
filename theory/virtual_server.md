Physical server — one machine, one IP, one port, one set of rules. One
   nginx process, one config. That's it.
   ==========================

   Virtual server — one machine pretending to be multiple servers. Same
  process, same IP, but multiple ports or hostnames, each with
  independent config. The "virtual" just means it's simulated in
  software, not separate hardware.

  ===============================

  in default.conf;  server 1 (port 8080) and server 2 (port 8081) are virtual servers
   — same process, different rules. A request to 8081 can never
  accidentally match a location defined only on 8080.
  ============================

   Why virtual servers exist at all:

  Before them, if you wanted an API and a static file server on the same
   machine you ran two separate processes. That's wasteful — two epoll
  loops, two sets of fds, two processes to manage. Virtual servers give
  you the isolation of separate servers with the efficiency of one
  process.
  