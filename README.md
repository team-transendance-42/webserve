*This project has been created as part of the 42 curriculum by pekatsar, rmengelb, nmattos-.*

# Description
The goal of this project is to create a simple web server in C++.

## Features
- Can use a given configuration file to set up the server
- Handles HTTP requests
- Supports HEAD, GET, POST, and DELETE methods
- Serves static files
- Executes CGI scripts (python)
- Is able to listen to multiple ports to deliver different content
- Allows users to upload files

## Server Configuration
The configuration file is used to set up the server. It allows users to specify which ports to listen to, what content to serve, and how to handle requests.\
The configuration file is structured as follows:

```
server {
	listen 		8080;
	host		127.0.0.1;
	server_name example.com;

	clientMaxBodySize 1048576;

	error_page 404 path/to/404.html;
	error_page 500 path/to/500.html;

	location {
		root			/path/to/root/;
		path			/path/to/location/;
		allowedMethod	GET POST DELETE;
	}
}
```

The configuration file can contain multiple server blocks, each with their own configuration. The server block specifies which port to listen to, the host IP address, the server name, etc. Inside a server block, you can define multiple location blocks, which tell the server how to handle requests for specific paths.

# Instructions
To compile, navigate to the root directory and run the following command:
```bash
make
```
To run the server with its default configuration, use the command below.
```bash
./webserv
```
Adding <config_file> results in the server using the given configuration file.
```bash
./webserv <config_file>
```
E.g
```bash
./webserv config/default.conf
```

## Running Tests
TODO

# Resources
[CGI in Python](https://www.geeksforgeeks.org/python/what-is-cgi-in-python/)\
[Nginx Beginner's Guide](https://nginx.org/en/docs/beginners_guide.html)\
[poll() or epoll()](https://gist.github.com/MangaD/a16e7e4caadb5427fd3b3e37c4d41ed4)\
[http](https://developer.mozilla.org/en-US/docs/Web/HTTP)\
[http methods](https://developer.mozilla.org/en-US/docs/Web/HTTP/Reference/Methods)\
[http status codes](https://developer.mozilla.org/en-US/docs/Web/HTTP/Reference/Status)\
[RFC http 7230](https://datatracker.ietf.org/doc/html/rfc7230)\
[RFC http 7231](https://datatracker.ietf.org/doc/html/rfc7231)\
[RFC http 9110](https://datatracker.ietf.org/doc/html/rfc9110)
