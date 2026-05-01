Shared Epoll (single event loop)


  1. Server takes a vector of configs, opens N listen sockets
  // Server.hpp
  std::vector<VirtualServer>  _vservers;        // each holds config +
  processor
  std::map<int, size_t>       _listenFdToIdx;   // listen fd → vserver
  index
  -------------
   2. init() loops and registers all listen fds with the ONE epoll
  for each vserver:
      fd = socket/bind/listen(vserver.config.port)
      _listenFdToIdx[fd] = i
      _epoll.add(fd, EPOLLIN)
------------------------

 3. tick() checks if the ready fd is a listen fd or client fd
  if (_listenFdToIdx.count(fd))
      _acceptClient(_listenFdToIdx[fd])   // new conn on vserver i
  else
      read/write the client as normal
------------------------

4. Client carries a pointer to its vserver's ProcessRequest
  // so the right config (error pages, body size, locations) is used
  Client(fd, &_vservers[i].processor)

  5. ConnectionManager calls client.processor->handle() instead of a
  shared one