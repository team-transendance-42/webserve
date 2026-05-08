nginx/webserver:
* read .conf file: parse, 
*run server(sets up: listen on url:portnum, locations, etc);
* the webserver runs the app: displaying the index page, 
*on clikc on a link there, send req to the server as a stream as a tcp from the socket, parses the req, looks for location, finds file and responses with header, status and body, and serves: dipslays requested static file in my case
=====================================
Read & parse .conf: The server reads its configuration file, parses settings (listen address/port, locations, root, error pages, etc.).
Setup: The server binds to the configured IP/port, sets up routes/locations, and (optionally) uses server_name for virtual hosting (to distinguish between multiple domains on the same IP/port).
Run & listen: The server waits for incoming TCP connections.
Request handling:
Browser/user clicks a link or enters a URL.
The browser sends an HTTP request (over TCP) to the server.
The server reads/parses the request, matches the URL to a location block.
It finds the file/resource to serve (for static files).
It builds an HTTP response (status, headers, body).
It sends the response back to the client.
Display: The browser receives the response and displays the page or file.
server_name is used for virtual hosting (serving different sites on the same server/IP. The browser does not know the server’s file system. It only knows the URL it requested. The server maps the URL to a file path (using its configuration and location blocks), reads the file, and sends it as the response body. The browser just renders whatever it receives, based on the headers.

