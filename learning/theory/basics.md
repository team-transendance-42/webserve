**What is a socket**

A socket is:
An endpoint for communication between two programs.
Think of it like:

Program A  ← socket →  Network  ← socket →  Program B
It's just a file descriptor created by the OS that allows communication.

On Linux everything is a file:
files, pipes, sockets,terminals
Sockets are just special file descriptors.

socket() system call
Creates communication endpoint.
int socket(int domain, int type, int protocol);

Example:
int server_fd = socket(AF_INET, SOCK_STREAM, 0);
AF_INET → IPv4
SOCK_STREAM → TCP
0 → default protocol
--------------------------

**What is TCP**

TCP (Transmission Control Protocol) is not a program or a file you can see. It is a protocol—a set of rules—built into your operating system’s networking stack (kernel). Physically, it is implemented as code inside the OS (Linux, Windows, etc.), not as a user program.
you use it by writing programs that send/receive data using sockets. The OS handles the TCP details for you.
When you call socket(AF_INET, SOCK_STREAM, 0), the OS creates a TCP socket. All TCP logic (connections, reliability, etc.) is handled by the OS kernel automatically. You just use the socket API; the TCP protocol runs invisibly under the hood.

TCP = Transmission Control Protocol.
It is:
Reliable, Ordered, Error-checked, Connection-based

When you open a website:
Browser ↔ TCP ↔ Server
TCP guarantees:
No data loss, Correct order, Automatic retransmission
Webserve uses TCP.
----------------------------------------------

**File descriptor*

An integer that represents an open resource.
0 → stdin
1 → stdout
2 → stderr
3 → socket
4 → another socket
int server_fd = socket(...);
--------------------------------

**bind()**

connects your socket to:
IP address, Port

Example:

bind(server_fd, (sockaddr*)&address, sizeof(address))

Without bind, OS assigns random port.

For server, you MUST bind.
----------------------------------

**listen()**

Tells OS: This socket will accept incoming connections.

Example:
listen(server_fd, 10);
10 = backlog (queue size)
-----------------------------------

**accept()**

When a client connects:
int client_fd = accept(server_fd, ...);

Important:
server_fd → listening socket
client_fd → new socket for communication
Now you communicate with client_fd.
-----------------------------------

**recv() / send()**

Receive data:
char buffer[1024];
recv(client_fd, buffer, 1024, 0);

Send data:
send(client_fd, "Hello", 5, 0);
That's how HTTP response is sent.
------------------------------------

Blocking vs non-blocking

Blocking
If no client connects:
accept() → waits forever
Program freezes.

Non-blocking
If no client connects:
accept() → returns immediately
Used for multiple clients.

You enable with:
fcntl(fd, F_SETFL, O_NONBLOCK);
stands for "file control." It is a system call in Unix/Linux used to manipulate file descriptors.
*F_SETFL tells fcntl to set file status flags.
*O_NONBLOCK makes the file descriptor non-blocking: operations like accept(), recv(), or read() return immediately if no data is available, instead of waiting.
------------------------------------

poll() / select() / epoll()

Used to handle MANY clients without threads.

poll()
Waits until:
socket ready to read
socket ready to write
if socket ready → read
if new connection → accept

Bigger projects use epoll() → more efficient for many clients.
-------------------------------

Big picture:

Normal server:
socket()
bind()
listen()
while (true)
{
    accept()
    recv()
    send()
}

Advanced server:
socket()
bind()
listen()
set non-blocking

while (true)
{
    poll()
    accept()
    recv()
    send()
}
--------------------------------

Mental Model
Web server is fundamentally an event loop that manages three things:

Config — what the server knows before any connection
Connections — state of each active client
Requests/Responses — data flowing through each connection

Everything else serves these three.
------------------------------------

The Config Layer (parsed once, read forever)
Location
Represents one location block in config.

struct Location {
    std::string             path;           // "/", "/images", "/cgi-bin"
    std::string             root;           // filesystem root for this location
    std::string             index;          // default file: "index.html"
    std::string             redirect;       // redirect target if set
    int                     redirect_code;  // 301, 302
    std::vector<std::string> allowed_methods; // ["GET", "POST"]
    bool                    autoindex;      // directory listing on/off
    size_t                  max_body_size;  // bytes, 0 = use server default
    std::string             cgi_extension;  // ".php", ".py"
    std::string             cgi_path;       // "/usr/bin/python3"
    std::string             upload_dir;     // where uploaded files go
};
---

ServerConfig
Represents one server block

struct ServerConfig {
    std::string              host;           // "127.0.0.1"
    int                      port;           // 8080
    std::vector<std::string> server_names;   // ["example.com", "www.example.com"]
    size_t                   max_body_size;  // default for all locations
    std::map<int, std::string> error_pages;  // {404: "./errors/404.html"}
    std::vector<Location>    locations;      // ordered, matched top-down
};
---

Config
Top-level, owns everything

class Config {
public:
    void                        parse(const std::string& path);
    const ServerConfig&         getServer(const std::string& host, int port) const;

private:
    std::vector<ServerConfig>   _servers;

    // parsing internals
    void    _parseServerBlock(std::ifstream& file);
    void    _parseLocationBlock(std::ifstream& file, ServerConfig& server);
    size_t  _parseSize(const std::string& val); // "10M" → 10485760
};

Key rule: getServer() matches by Host header + port. If multiple servers share a port, the Host header selects which one. Default to first match.
---

The Network Layer
Server
Owns the listening socket for one host:port pair.

class Server {
public:
    Server(const ServerConfig& config);
    ~Server();

    void        init();         // socket(), bind(), listen()
    int         getFd() const;  // the listening fd for select/poll
    int         accept();       // returns new client fd
    const ServerConfig& getConfig() const;

private:
    int             _fd;
    ServerConfig    _config;
    sockaddr_in     _addr;
};

Why separate from config? Because config is data, Server is a live socket. Multiple ServerConfig blocks on the same port share one Server — your event loop must handle this.
---

The Connection Layer (the heart)
Connection
One per connected client. Owns the entire lifecycle of that client's TCP connection.

class Connection {
public:
    enum State {
        READING_REQUEST,
        PROCESSING,
        WRITING_RESPONSE,
        CLOSED
    };

    Connection(int fd, const std::vector<ServerConfig>& configs);
    ~Connection();

    void        read();             // called when fd is readable
    void        write();            // called when fd is writable
    bool        isDone() const;     // true → remove from event loop
    int         getFd() const;
    time_t      getLastActivity() const;

private:
    int                             _fd;
    State                           _state;
    std::vector<ServerConfig>       _configs;   // all configs (for Host matching)
    time_t                          _last_activity;

    HttpRequest                     _request;
    HttpResponse                    _response;

    std::string                     _read_buf;  // raw incoming bytes
    std::string                     _write_buf; // raw outgoing bytes

    void    _process();             // build response from request
    void    _selectConfig();        // match Host header → ServerConfig
    void    _selectLocation();      // match URI → Location
};

Critical design: read() and write() must be non-blocking. They do one chunk of work and return. State machine advances between calls.
---

The HTTP Layer
HttpRequest

class HttpRequest {
public:
    enum ParseState {
        REQUEST_LINE,
        HEADERS,
        BODY,
        COMPLETE,
        ERROR
    };

    void            feed(const std::string& data);  // push raw bytes in
    bool            isComplete() const;
    bool            hasError() const;
    int             getErrorCode() const;           // 400, 413, etc.

    // accessors (only valid after isComplete())
    const std::string&  getMethod() const;
    const std::string&  getUri() const;
    const std::string&  getPath() const;       // URI without query string
    const std::string&  getQuery() const;      // query string
    const std::string&  getVersion() const;
    const std::string&  getHeader(const std::string& name) const;
    const std::string&  getBody() const;
    bool                hasHeader(const std::string& name) const;

private:
    ParseState                          _state;
    std::string                         _raw;
    std::string                         _method;
    std::string                         _uri;
    std::string                         _path;
    std::string                         _query;
    std::string                         _version;
    std::map<std::string, std::string>  _headers;
    std::string                         _body;
    size_t                              _content_length;
    bool                                _chunked;
    int                                 _error_code;

    void    _parseRequestLine();
    void    _parseHeaders();
    void    _parseBody();
    void    _parseChunked();
    void    _decodeUri();   // percent-decode %20 etc.
};

Key insight: feed() is called every time new bytes arrive. It tries to advance _state as far as possible, then stops and waits for more data. Never assume you get a complete request in one read.
---

HttpResponse

class HttpResponse {
public:
    HttpResponse();

    void    setStatus(int code);
    void    setHeader(const std::string& name, const std::string& value);
    void    setBody(const std::string& body);
    void    setBodyFromFile(const std::string& path);   // reads file into body
    void    build();    // assembles _raw from status + headers + body

    const std::string&  getRaw() const;     // the bytes to send
    bool                isBuilt() const;

    // convenience builders
    static HttpResponse makeError(int code, const ServerConfig& config);
    static HttpResponse makeRedirect(int code, const std::string& location);

private:
    int                                 _status_code;
    std::string                         _status_text;
    std::map<std::string, std::string>  _headers;
    std::string                         _body;
    std::string                         _raw;
    bool                                _built;

    static std::string  _statusText(int code);
};
---

The Handler Layer
These turn a complete HttpRequest into an HttpResponse. Each handles one case.

class RequestHandler {
public:
    virtual ~RequestHandler() {}
    virtual HttpResponse handle(const HttpRequest& req,
                                const ServerConfig& config,
                                const Location& location) = 0;
protected:
    std::string _resolvePath(const HttpRequest& req, const Location& loc);
};
---

StaticHandler
Serves files from disk.

class StaticHandler : public RequestHandler {
public:
    HttpResponse handle(const HttpRequest& req,
                        const ServerConfig& config,
                        const Location& location) override;
private:
    HttpResponse    _serveFile(const std::string& path);
    HttpResponse    _serveDirectory(const std::string& path, const Location& loc);
    std::string     _getMimeType(const std::string& path);
    bool            _pathIsAllowed(const std::string& path, const Location& loc);
};
---

CgiHandler
Forks a process, pipes stdin/stdout.

class CgiHandler : public RequestHandler {
public:
    HttpResponse handle(const HttpRequest& req,
                        const ServerConfig& config,
                        const Location& location) override;
private:
    std::string             _buildEnv(const HttpRequest& req,
                                      const Location& loc);
    std::vector<char*>      _makeEnvp(const HttpRequest& req,
                                      const Location& loc,
                                      const ServerConfig& config);
    HttpResponse            _parseCgiOutput(const std::string& raw);
    
    static const int        TIMEOUT_SECONDS = 5;
};

etc....




---
actual loop:
void EventLoop::run() {
    while (true) {
        fd_set reads, writes;
        int max_fd;
        _buildFdSets(reads, writes, max_fd);

        timeval timeout = {TIMEOUT_SEC, 0};
        int ready = select(max_fd + 1, &reads, &writes, NULL, &timeout);

        if (ready < 0)  // error, check errno
            continue;

        // check listening sockets first
        for (auto& server : _servers)
            if (FD_ISSET(server.getFd(), &reads))
                _handleNewConnection(server);

        // then existing connections
        _handleExistingConnections(reads, writes);

        // drop idle clients
        _reapTimedOutConnections();
    }
}




