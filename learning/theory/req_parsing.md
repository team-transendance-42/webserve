HTTP Request Parsing
we are parsing: raw bytes → structured object
Browser sends this over the socket (raw bytes):
──────────────────────────────────────────────
POST /upload?user=42 HTTP/1.1\r\n
Host: localhost:8080\r\n
Content-Type: application/json\r\n
Content-Length: 27\r\n
Connection: keep-alive\r\n
\r\n
{"name":"webserv","port":80}
──────────────────────────────────────────────
         │
         ▼
    HttpRequest object:
    {
      method:  POST
      path:    /upload
      query:   user=42
      version: HTTP/1.1
      headers: { Host, Content-Type, Content-Length, Connection }
      body:    {"name":"webserv","port":80}
    }