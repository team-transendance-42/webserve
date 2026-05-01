In a web server, the connection between client, req (request), and loc (location) works as follows:

client: Represents the network connection to a user (browser or API client). It holds connection state, buffers, and is used to send/receive data.
req (HttpRequest): Represents the parsed HTTP request sent by the client. It contains the HTTP method, path, headers, body, etc.
loc (Location): Represents the server configuration block that matches the request path. It defines rules, permissions, root directory, allowed methods, etc.
===============================================

The client connects to the server and sends an HTTP request.
The server reads the request from the client and parses it into an HttpRequest object (req).
The server matches the request path to a Location block (loc) in its configuration.
The server uses loc to determine how to handle the request (permissions, file paths, etc.).
The server processes the request (e.g., serve a file, handle upload, delete, etc.) and writes the response back to the client.
=====================================
The browser (like Chrome, Firefox) or an API client (a tool or program that makes HTTP requests, such as Postman, curl, or a backend service) acts as the client.
HTTP/S is a protocol—a set of rules for how clients and servers communicate over the internet.
The client (browser or API client) sends an HTTP request (like GET, POST) to the web server. This request includes a URL, headers, and sometimes a body (for POST/PUT).
The web server is an executable program running on a computer. It listens for incoming HTTP requests on a network port (like 80 or 443).
When the server receives a request, it parses (reads and interprets) the HTTP request.
The server then decides what to do: it might serve a static file (like an HTML page or image), run some code (like a script or backend logic), or fetch data from a database or another server.
The server does not usually “get info from the internet” unless it’s programmed to (for example, as a proxy or when calling external APIs). Most often, it serves files or data it already has access to.
The server builds an HTTP response (status code, headers, body) and sends it back to the client.
The browser receives the response and displays the page or data to the user. An API client processes the data as needed.

A web server is a program that receives HTTP requests from clients (browsers or API tools), processes them (serves files, runs code, or fetches data), and sends back HTTP responses.

