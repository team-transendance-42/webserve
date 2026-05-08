. Server .conf file
Purpose: Configures the web server (like nginx, Apache, or your own server).
Who uses it: The server software reads it at startup or reload.
Where defined: On the server, usually as a file (e.g., nginx.conf, default.conf).
Minimal fields:
listen (port)
server_name (host)
root (document root)
location / { ... }
index (default file)

server {
    listen 8080;
    server_name localhost;
    location / {
        root ./www;
        index index.html;
    }
}
---------------------
2. Client HTTP request
Purpose: Sent by the client (browser, curl, etc.) to request a resource from the server.
Who uses it: The client sends it; the server receives and parses it.
Where defined: Sent over the network, not stored as a file.
Minimal fields:
Request line: METHOD PATH VERSION (e.g., GET /index.html HTTP/1.1)
Host header (required for HTTP/1.1)
(Optional) Other headers

GET /index.html HTTP/1.1
Host: localhost:8080

------------------------

The .conf file configures the server (on disk, read by server).
The HTTP request is sent by the client (over the network, parsed by server).
Both are required for a working web server, but they are used in different places and by different parties.


===================================
Standard for nginx-style config files:
  - Each server {} block is a virtual host — different port or
  server_name for name-based vhosting
  - Locations are ordered most-specific first (longest prefix wins at
  runtime)
  - deny all guards sensitive paths
  - Error pages defined at server level, not per-location
  - clientMaxBodySize set only where it differs from server default
======================================

 Server 2 — API (port 8081)
  - clientMaxBodySize 524288 — API requests should be small; a tight
  limit prevents someone posting a 10 MB JSON blob
  - Two locations: /api/v1 (read+write) and /api/health (read-only) —
  health endpoints are a standard practice so load balancers and
  monitoring tools can probe liveness without hitting real endpoints
  - autoindex off everywhere — you never want directory listings on an
  API server

  Server 3 — Static (port 8082)
  - clientMaxBodySize 0 — no uploads accepted at all; pure read server
  - /static — compiled frontend assets; autoindex off because clients
  always request exact filenames
  - /downloads — autoindex on is intentional and correct here: a
  downloads directory exists specifically so users can browse it
  - Separate port is standard when you want different caching,
  rate-limiting, or CDN rules for static vs dynamic traffic

  General nginx-style conventions followed:
  - Section header comments say the purpose, not just the port number
  - error_page at server level, not duplicated per-location
  - clientMaxBodySize only overridden where it differs from the default
  - Inline comments explain the why behind non-obvious choices
  ============================================
  the only truly compulsory lines
  per server block are:

  server {
      listen 8080;        # which port — required
      location / {        # catch-all — required to serve anything
          root ./www;     # where files live — required
      }
  }

  Everything else (host, server_name, index, error_page,
  clientMaxBodySize) has defaults. Without listen the server doesn't
  know what port. Without location / + root there's nothing to serve.