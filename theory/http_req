GET /index.html HTTP/1.1
Host: example.com
User-Agent: Mozilla/5.0
Accept: text/html

The first line is the request line:
METHOD URI HTTP_VERSION

Followed by headers (each on a new line)
Ends with a blank line (CRLF) 

before the body (if any)
The official syntax is defined in the HTTP RFCs:
https://www.rfc-editor.org/rfc/rfc9110
-------------------------------

HTTP request received by the web server.
Handles parsing of raw HTTP request data (from client sockets) into structured fields: method, URI, headers, body, etc.

The server receives raw data from a client (browser, API, etc.).
This raw data is passed to the HttpRequest class for parsing.
After parsing, the structured HttpRequest object is used by other components (like HttpResponse or routing logic) to generate a response.

The config file (parsed by ServerConfig) is used elsewhere to determine server behavior (routes, allowed methods, etc.).
HttpRequest only parses the incoming request; it does not handle configuration.

How does config interact?

After parsing, the server uses the HttpRequest object and consults the already-parsed config (via ServerConfig) to decide:
Which location block matches the request URI
What permissions, root, or CGI settings apply
How to handle the request (serve file, run CGI, return error, etc.)
---------------------------------
Summary:
HttpRequest.hpp is the entry point for transforming raw client data into a usable request object.
It does not interact directly with configuration files; config is parsed separately and used later in request handling.
The flow is: client sends request → server receives → HttpRequest parses → server logic uses parsed request + config to generate response.
------------------------------
req parsing:

The server receives raw HTTP request data from the client.
HttpRequest.cpp parses this data into structured fields: method, URI, headers, body.

tokenization is needed: the parser splits the request line, headers, and body using delimiters (spaces, CRLF, colon).
This helps extract method, path, protocol, header keys/values, and body content.

parseRequest(): Main entry, orchestrates parsing of the request.
parseRequestLine(): Extracts method, URI, HTTP version from the first line.
parseHeaders(): Parses headers into a map/dictionary.
parseBody(): Handles body extraction (for POST/PUT).
tokenize(): Utility for splitting strings (may be used internally).
---------------------------------------
Security Considerations:

Validate method, URI, and headers for allowed values.
Limit header/body size to prevent buffer overflows.
Sanitize input to avoid injection attacks (e.g., CRLF injection).
Check for malformed requests and reject them.
Avoid directory traversal (../) in URI.
----------------------------------------

CRLF injection
CRLF = \r\n (Carriage Return, Line Feed)
\r\n is used to mark the end of a line in HTTP and many network protocols.
-------------------------------------

Example of CRLF Injection:

Suppose a server builds a header like this:
std::string header = "X-User: " + user_input + "\r\n";

If user_input is:
attacker\r\nSet-Cookie: session=evil

The resulting HTTP response will be:
HTTP/1.1 200 OK
X-User: attacker
Set-Cookie: session=evil
...
The attacker has injected a new header!

If not sanitized, the server will process Set-Cookie as a real header.

our code prevents this by rejecting any header key or value containing \r or \n, blocking such attacks.