To trigger a forbidden access (403):

Try to access a directory with autoindex off and no index file:

Example: Visit http://localhost:8080/files/ when your config has:
and there is no index.html in ./www/one/files.

Try to access a file or directory with filesystem permissions set to deny access (chmod 000).

Any of these will trigger your custom 403 Forbidden page.