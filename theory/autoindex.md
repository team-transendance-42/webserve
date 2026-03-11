Autoindex is a feature in web servers that automatically generates and displays a directory listing (an HTML page) when a user requests a directory URL that does not have an index file (like index.html).

Example:
User requests: http://localhost/one/errors/
If there's no index.html in errors and autoindex is enabled, the server responds with a page listing all files and folders in that directory.

What does autoindex do?
Reads the directory contents.
Generates an HTML page with links to each file/subdirectory.
Lets users browse files directly from the browser.

When is autoindex used?
When a directory is requested and:
No index file exists.
Autoindex is enabled in the server config.