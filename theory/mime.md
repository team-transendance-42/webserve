MIME type (Multipurpose Internet Mail Extensions) tells the browser what kind of file is being served, so it knows how to handle it.

Example:
.html → text/html
.jpg → image/jpeg
.css → text/css
.js → application/javascript

Why does the server set MIME types?
So browsers display files correctly (HTML as web pages, images as images, etc.).
Prevents browsers from misinterpreting files.
How does your server set MIME types?
It checks the file extension.
Returns the correct Content-Type header in the HTTP response.
Example code from

When is autoindex disabled?
The server returns a 403 Forbidden or a custom error page instead of a directory listing.
