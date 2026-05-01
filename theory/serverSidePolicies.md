client_max_body_size 1m;
Limits the maximum allowed size of the client request body (default 1MB).

keepalive_timeout 65;
How long to keep a connection open after completing a request.

limit_conn
Limits the number of connections per key (e.g., per IP).

limit_rate
Limits the rate of response transmission to a client.

large_client_header_buffers
Sets the maximum number and size of buffers for large headers.