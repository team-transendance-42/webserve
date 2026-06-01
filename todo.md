tests:
telnet, netcat, nc, curl req

very long body but allowed what happens
piping cgi

pathtraversal
execute files with not allowed permissions(delete, post etc)

=============
Telnet
--------------
 is a network protocol and command-line tool that lets you connect to remote computers over TCP/IP. It opens a raw text-based connection to a specified host and port, allowing you to manually send and receive data. It’s often used for testing servers, debugging network services, or interacting with devices that accept plain text commands.
Opens a raw TCP connection to a host:port.
Lets you type and send data manually.
Good for testing text-based protocols (HTTP, SMTP, etc.).
---------
nc (netcat)
------------
What it does:

Opens TCP or UDP connections, listens for connections, or sends data.
More flexible than telnet (can send files, listen, use UDP, etc.).
Good for scripting, port scanning, and raw data transfer.
nc 127.0.0.1 8080

Type your request and press Enter twice (for HTTP).
To send a file:
nc 127.0.0.1 8080 < myfile.txt

To listen for connections
nc -l 1234
----------
curl
--------------
What it does:

Command-line tool for transferring data with URLs.
Supports HTTP, HTTPS, FTP, and many more protocols.
Handles headers, cookies, authentication, file uploads, etc.
Not interactive—sends requests and prints responses.
curl http://127.0.0.1:8080/

curl -X POST -d "hello" http://127.0.0.1:8080/api

add headers:
curl -H "X-Test: 1" http://127.0.0.1:8080/

set output to file:
curl -o output.html http://127.0.0.1:8080/

===================
telnet 127.0.0.1 8080
enter
GET / HTTP/1.1
Host: 127.0.0.1
(Press Enter twice after the last header to send the request.)
==========
POST /api HTTP/1.1
Host: 127.0.0.1
Content-Length: 5

hello
===============
bugs
=================



  ---
  5. Absolute-form URI with query string but no path loses the query
  string

  File: srcs/HttpRequest.cpp:1-172

  What you need to know: _parsePath handles absolute-form URIs from
  proxies. For http://example.com?q=search (no / path), raw.find('/',
  pos) returns npos and target = "/". The entire query string is
  silently discarded.

  What to change:
  if (auth_end != std::string::npos) {
      target = raw.substr(auth_end);  // keeps ?query and #fragment
  } else {
      // No path segment — check for bare query
      size_t q = raw.find('?', raw.find("//") + 2);
      target = (q != std::string::npos) ? raw.substr(q) : "/";  // e.g.
  "?q=1" → path="/", qs="q=1"
      // Actually set path to "/" and query separately
      target = "/";  // then extract query below separately
  }
  The cleanest fix is to extract the query component before stripping
  the authority, not after.

  Pros: Correct QUERY_STRING for CGI when proxied without a path
  component.

  Cons: Unusual real-world case; proxies almost always include at least
  /.

  Edge cases: http://[::1]:8080/path — IPv6 in Host. raw.find("//")
  finds // at position 5, +2 = 7, raw.find('/', 7) searches from [ and
  correctly finds /path. Safe.

  ---
  6. Time-of-check/time-of-use race in _saveUpload

  File: srcs/ProcessRequest.cpp:181–186

  What you need to know: The collision-avoidance loop calls stat() to
  check if a file exists, then open() to create it. Between those two
  calls another request can create the same file, causing the second
  upload to silently overwrite it.

  What to change: Use O_CREAT | O_EXCL flags to atomically fail if the
  file already exists:
  int fd = open(fullPath.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0644);
  if (fd == -1 && errno == EEXIST) { /* try next suffix */ continue; }

  Pros: Eliminates race; atomic creation is the correct POSIX pattern.

  Cons: More complex than std::ofstream; need to wrap fd in RAII or
  handle manually.

  Edge cases: Very high suffix numbers in tight concurrent loops
  (unlikely in practice for a 42 project evaluation).

  ---
  7. CGI response with Status: 204 still serializes with a body

  File: srcs/ProcessRequest.cpp:678–679

  What you need to know: setBody(body, "text/html") is called first
  (sets body + Content-Length), then _applyCgiHeaders applies Status:
  204. The status is updated, but the body is never cleared. RFC 9110
  §6.4.1: 204 responses MUST NOT include a message body.

  What to change: Apply CGI headers before setting the body, or clear
  the body when the status indicates no content:
  _applyCgiHeaders(headerSection, response);           // status set
  first
  response.setBody(body, "text/html");                 // body set after
  // Then in serialize(): if status is 204/304/1xx, skip body
  Or, after _applyCgiHeaders, check response.statusCode and clear the
  body if it's 204/304.

  Pros: Correct behavior for CGI scripts that signal no-content
  responses.

  Cons: Requires knowing which status codes forbid bodies; can be a
  short allowlist (204, 304, 1xx).

  Edge cases: 304 Not Modified also must not include a body per RFC 9110
  §15.4.5. Same fix covers both.

  ---
  8. content_length() guard accepts SIZE_MAX - 1 as a valid length

  File: srcs/HttpRequest.cpp:302

  What you need to know: The check n > static_cast<unsigned long
  long>(INVALID - 1) rejects only values > SIZE_MAX - 1, meaning
  SIZE_MAX - 1 itself is returned as a valid content length (≈18 EB on
  64-bit). The MAX_BODY_SIZE cap (10 MB) at line 76 saves you from
  acting on it, but the guard logic is off-by-one.

  What to change:
  if (*end != '\0' || errno == ERANGE || n > static_cast<unsigned long
  long>(INVALID))
  But since INVALID == SIZE_MAX == ULLONG_MAX on 64-bit, n > ULLONG_MAX
  is always false — strtoull already returns ULLONG_MAX with ERANGE on
  overflow. Simplify to:
  if (*end != '\0' || errno == ERANGE || n >
  std::numeric_limits<size_t>::max() / 2)
      return INVALID;
  Or just rely on the MAX_BODY_SIZE check exclusively and remove the
  redundant upper bound.

  Pros: Cleaner logic; no latent misuse if MAX_BODY_SIZE is ever
  removed.

  Cons: Low practical risk since MAX_BODY_SIZE catches it.

  Edge cases: 32-bit platforms where size_t is 32-bit and unsigned long
  long is 64-bit — the current check would be wrong there too. Worth
  fixing for portability.

  ---
  9. DELETE handler's path check returns 400 for mismatched prefix
  locations

  File: srcs/ProcessRequest.cpp:265

  What you need to know: The guard urlPath[loc.path.size()] != '/'
  protects against DELETE /files_autos when loc.path = "/files". If
  matchLocation uses prefix matching, a request for /files_autos/doc.txt
  could match location /files, and the DELETE handler would return 400
  instead of 404 — confirming the existence of /files to a client doing
  reconnaissance.

  What to change: Confirm that matchLocation never matches a location
  whose loc.path is a mere prefix of the request path (i.e., it requires
  an exact match or a path ending with /). If it does, the 400 response
  is a real information leak.

  Pros of fix: Correct error code (404 instead of 400) for non-matching
  paths.

  Cons: Requires understanding of matchLocation logic (not changed in
  this branch).

  Edge cases: URL-encoded slashes (%2F) in the path could bypass the !=
  '/' check on the raw string. The DELETE handler doesn't decode
  percent-encoding before this check.

  ---
  10. _resolvePathStatOrError called with empty filepath when path is
  outside root

  File: srcs/ProcessRequest.cpp:477

  What you need to know: _resolveFilePath returns "" when
  _canonicalizeWithinRoot fails (path traversal, nonexistent file). Then
  stat("", &st) is called with an empty string. stat("") fails with
  ENOENT → 404. This works correctly by accident, but only because
  ENOENT is the failure errno. If the OS ever returns a different errno
  for stat("") (implementation-defined), you'd get a 403 or 500 for a
  path-traversal attempt.

  What to change: Add an explicit guard before calling stat:
  if (filepath.empty()) {
      client.writeBuf = ErrorResponseBuilder::buildErrorResponse(404,
  cfg).serialize();
      return false;
  }

  Pros: Explicit, not relying on OS behavior for empty string stat.

  Cons: Minor defensive hardening; low real-world impact.

  Edge cases: On Linux, stat("") returns ENOENT reliably. On some
  embedded systems it may differ.

  ---
  Summary table (most severe first):

  ┌─────┬─────────┬───────────────────────────┬────────────────────┐
  │  #  │ Severit │           File            │    What breaks     │
  │     │    y    │                           │                    │
  ├─────┼─────────┼───────────────────────────┼────────────────────┤
  │     │ Critica │ ProcessRequest.cpp:497–50 │ HEAD returns body  │
  │ 1   │ l       │ 7                         │ for all non-static │
  │     │         │                           │  paths             │
  ├─────┼─────────┼───────────────────────────┼────────────────────┤
  │     │         │                           │ debugPrint() UB    │
  │ 2   │ High    │ HttpRequest.cpp:369       │ when               │
  │     │         │                           │ method==UNKNOWN    │
  ├─────┼─────────┼───────────────────────────┼────────────────────┤
  │     │         │                           │ Allow: GET, HEAD,  │
  │ 3   │ High    │ ProcessRequest.cpp:99–101 │ HEAD in 405 (RFC   │
  │     │         │                           │ violation)         │
  ├─────┼─────────┼───────────────────────────┼────────────────────┤
  │     │         │                           │ CGI gets REQUEST_M │
  │ 4   │ Medium  │ ProcessRequest.cpp:598    │ ETHOD=HEAD instead │
  │     │         │                           │  of GET            │
  ├─────┼─────────┼───────────────────────────┼────────────────────┤
  │     │         │                           │ Proxy absolute-URI │
  │ 5   │ Medium  │ HttpRequest.cpp:1-172   │  drops query       │
  │     │         │                           │ string             │
  ├─────┼─────────┼───────────────────────────┼────────────────────┤
  │     │         │ ProcessRequest.cpp:181–18 │ Concurrent uploads │
  │ 6   │ Medium  │ 6                         │  can overwrite     │
  │     │         │                           │ each other         │
  ├─────┼─────────┼───────────────────────────┼────────────────────┤
  │ 7   │ Medium  │ ProcessRequest.cpp:678–67 │ CGI Status:204     │
  │     │         │ 9                         │ still sends body   │
  ├─────┼─────────┼───────────────────────────┼────────────────────┤
  │     │         │                           │ SIZE_MAX-1         │
  │ 8   │ Low     │ HttpRequest.cpp:302       │ accepted as valid  │
  │     │         │                           │ content length     │
  ├─────┼─────────┼───────────────────────────┼────────────────────┤
  │     │         │                           │ DELETE returns 400 │
  │ 9   │ Low     │ ProcessRequest.cpp:265    │  instead of 404    │
  │     │         │                           │ for mismatched     │
  │     │         │                           │ prefix             │
  ├─────┼─────────┼───────────────────────────┼────────────────────┤
  │     │         │                           │ Empty filepath     │
  │ 10  │ Low     │ ProcessRequest.cpp:477    │ relies on OS for   │
  │     │         │                           │ correct ENOENT     │
  └─────┴─────────┴───────────────────────────┴────────────────────┘

