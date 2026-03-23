RFC defines the status codes and their meaning.
-------------------------------------------------------------------------

400 Bad Request
malformed request, bad syntax, invalid headers, broken request line. RFC 9110 defines 400 for client-side request problems.

403 Forbidden
file exists but access is denied, directory listing forbidden, CGI/script not allowed.

404 Not Found
resource/path does not exist.

405 Method Not Allowed
method not allowed for that route/location. You should usually also send an Allow header for the permitted methods. RFC 9110 defines 405 and its Allow expectation.

408 Request Timeout
client took too long to send request.

411 Length Required
relevant when request body is expected/required but Content-Length is missing and you do not support chunked handling for that case.

413 Content Too Large
for 42, this is the classic one for clientMaxBodySize. RFC 9110 uses the name Content Too Large.

414 URI Too Long
request target/path too long.

415 Unsupported Media Type
sometimes useful for uploads/CGI handling, though many 42 projects never need it.

431 Request Header Fields Too Large
if headers are too large. Defined in RFC 9110.
-------------------------------------------------------------------

500 Internal Server Error
generic server failure.

501 Not Implemented
method not supported by your server at all, or feature not implemented.

502 Bad Gateway
mostly for proxy/gateway behavior, not needed for this project.

503 Service Unavailable
temporary overload or unavailable service.

504 Gateway Timeout
also mostly for gateway/proxy cases, usually not needed for plain 42 webserv. RFC discussions around gateway errors treat these as intermediary/gateway cases.

505 HTTP Version Not Supported
request uses unsupported HTTP version.
----------------------------------------------------------------------

non-error statuses

200 OK
201 Created
204 No Content
301 Moved Permanently
302 Found
303 See Other often useful after POST/CGI
307 Temporary Redirect
304 Not Modified (for conditional caching logic)
--------------------------------------------------------------------

parse fail → 400

file missing → 404

method not allowed in config → 405

body too big → 413

timeout waiting for request → 408

CGI/server crash/failure → 500

method not implemented by server → 501

HTTP/1.2 or weird version → 505

no permission/read forbidden → 403
---------------------------------------------------------------------

