most common and expected for a basic web server.

## 400 Client err
400 Bad Request
401 Unauthorized
403 Forbidden
404 Not Found
405 Method Not Allowed
408 Request Timeout
413 Payload Too Large
414 URI Too Long

## 500 Server err
500 Internal Server Error
501 Not Implemented
502 Bad Gateway
503 Service Unavailable
504 Gateway Timeout

## 300 rediractions
301 Moved Permanently
302 Found
304 Not Modified

Documentation sources:

RFC 7231 (HTTP/1.1 Semantics and Content): Official HTTP status codes and their meaning.
RFC 2616 (older, but still referenced for HTTP/1.1).
Nginx documentation: Shows practical handling and examples for each error code.
Apache documentation: Also useful for practical error handling.

For official definitions, always check the RFCs (especially RFC 7231). For practical implementation and examples, Nginx docs are helpful.