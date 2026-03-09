 How a C++17 Web Server Works (for a 12-year-old)
Think of it like a pizza shop:

Your server is the shop
The browser is the customer calling in
HTTP is the language they speak on the phone
-------------------------------------------------

Step 1 — Create a Socket (Open the phone line)

int server_fd = socket(AF_INET, SOCK_STREAM, 0);

A socket is like plugging in a telephone. AF_INET = use the internet. SOCK_STREAM = reliable two-way conversation (TCP).
--------------------------------------------------

Step 2 — Bind (Give the shop an address)

sockaddr_in addr{};
addr.sin_family = AF_INET;
addr.sin_port   = htons(8080);       // port number
addr.sin_addr.s_addr = INADDR_ANY;   // accept from anyone

bind(server_fd, (sockaddr*)&addr, sizeof(addr));

htons() converts the port number to network byte order (big-endian). Without this, port numbers get scrambled between machines.
------------------------------------------------

Step 3 — Listen (Start taking calls)

listen(server_fd, 10);  // queue up to 10 waiting customers
-----------------------------------------------

Step 4 — Accept (Pick up the phone)

int client_fd = accept(server_fd, nullptr, nullptr);
accept() blocks (waits) until a browser connects. It returns a new file descriptor just for that one client — the original keeps listening for more.
----------------------------------------------

Step 5 — Read the HTTP Request (Hear the order)

char buf[4096] = {};
read(client_fd, buf, sizeof(buf));
std::string request(buf);
```
A raw HTTP request looks like:
```
GET /index.html HTTP/1.1
Host: localhost:8080

You need to parse the first line to know what page they want.
----------------------------------------------

Step 6 — Parse the Request (Decode the order)

// Extract method + path from first line
std::istringstream ss(request);
std::string method, path, version;
ss >> method >> path >> version;
// method = "GET", path = "/index.html"
----------------------------------------------

Step 7 — Build the HTTP Response (Make the pizza)

std::string body = "<h1>Hello!</h1>";
std::string response =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Content-Length: " + std::to_string(body.size()) + "\r\n"
    "\r\n" +                          // blank line = end of headers
    body;
---------------------------------------------

Step 8 — Send the Response (Deliver the pizza)

send(client_fd, response.c_str(), response.size(), 0);
---------------------------------------------

Step 9 — Close the Connection (Hang up)

close(client_fd);
---------------------------------------------

Step 10 — Loop Forever (Keep the shop open)

while (true) {
    int client_fd = accept(server_fd, nullptr, nullptr);
    // handle client...
    close(client_fd);
}

