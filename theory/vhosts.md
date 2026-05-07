multiple domains on the same IP/port

==================================
  Step 1 — Multiple configs share one port in Server

  All ServerConfig objects in _configs share the same listening socket. Multiple configs = multiple virtual hosts on that port.
--------------------
  Step 2 — Every request enters _selectConfig in ProcessRequest

      std::string host = req.getHeader("host");  // e.g. "api:8080"
      // strip ":port" → "api"
      ...
  This runs on every single request before any routing logic.
-----------------------
  Step: HTTP/1.1 enforces Host header in ProcessRequest

  if (req.version == "HTTP/1.1" && !req.hasHeader("Host")) {
      // → 400 Bad Request
  Without Host:, vhost dispatch is impossible, so you correctly reject it.

  ---
  Concrete example:

  api config:    host=127.0.0.1, port=8080, server_names=["api"]
  static config: host=127.0.0.1, port=8080, server_names=["static"]

  Both bind to port 8080. Then:
  curl -H "Host: api"    http://127.0.0.1:8080/  → api config selected    → api routes/root
  curl -H "Host: static" http://127.0.0.1:8080/  → static config selected → static files root
  curl -H "Host: ???"    http://127.0.0.1:8080/  → Pass 3 fallback        → _configs[0]

  ---
  Summary of data flow

  TCP packet arrives
      → _acceptClient (one fd, one Client)
      → epoll fires EPOLLIN
      → readClient → HttpRequest::feed() builds parsed request
      → epoll fires EPOLLOUT
      → writeClient → ProcessRequest::handle(client)
                         → _selectConfig(req)   ← VHOST DISPATCH HERE
                             reads Host: header
                             matches server_name
                             returns the right ServerConfig
                         → rest of routing uses that config's locations/root/etc.