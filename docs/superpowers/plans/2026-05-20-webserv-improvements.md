# Webserv Improvement Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Incrementally improve the webserv codebase through four waves — dead code removal, bug fixes, security hardening, and polish — with A2/A3 (upload consolidation + CGI dedup) deliberately deferred to last.

**Architecture:** Four sequential waves. Each wave is independently reviewable. Implementer does NOT commit — all commits are left to the developer. Tests are `make` + `curl`/`nc` since there is no unit test framework.

**Tech Stack:** C++11, Linux epoll, POSIX sockets, fork/execve for CGI, Makefile build system

> ⚠️ **NOTE:** Tasks 23 and 24 (A2 and A3) are intentionally placed last. Fix everything else first.
> ⚠️ **NO GIT COMMITS** — do not commit anything. Leave all staging and committing to the developer.

---

## File Map

| File | Waves that touch it | What changes |
|---|---|---|
| `includes/config/Config.hpp` | A | Remove `CgiConfig`, remove unused function declarations, remove stale TODO comments |
| `includes/Server.hpp` | A, D | Remove `_running` field, reduce `POLL_TIMEOUT`, clean duplicate includes |
| `includes/HttpRequest.hpp` | A | Add `NOT_IMPLEMENTED` to `Method` enum |
| `includes/HttpResponse.hpp` | A, B | Declare `make_501()`, `make_504()` |
| `srcs/HttpRequest.cpp` | A | Recognise HEAD/OPTIONS/PUT/PATCH as `NOT_IMPLEMENTED` |
| `srcs/HttpResponse.cpp` | A, B | Add reason strings for 201/204/501/504; add `make_501()`, `make_504()` |
| `srcs/ProcessRequest.cpp` | A, B, C, A3 | Fix redirect range, fix 504 ordering, canonicalise index, type enforcement |
| `srcs/CgiExecutor.cpp` | A, B | Wrap helpers in anonymous namespace, fix double `waitpid` |
| `srcs/Server.cpp` | A, B | Remove `_running` ref, fix blocking send in timeout handler, fix use-after-free |
| `srcs/config/ConfigParser.cpp` | A, C | Overflow-safe `toUnsigned`, port range check, `upload_enabled`/`upload_allowed_types` |
| `srcs/ErrorResponseBuilder.cpp` | B | Add case 504, 415 |
| `srcs/StaticFileHandler.cpp` | D | Add modern MIME types |
| `srcs/UploadHandler.cpp` | C | Fix path traversal (realpath-based check) |
| `main.cpp` | B | Fix server leak on `init()` throw |
| `Makefile` | A | Remove UploadHandler from sources |
| `.gitignore` | A | Exclude `*.o` build artifacts |
| `default.conf` (all .conf files) | C | Add `deny_all` location for `/secrets` |
| `srcs/UploadHandler.cpp` | A1 | **DELETE** |
| `includes/UploadHandler.hpp` | A1 | **DELETE** |

---

## Wave A — Refactor & Dead Code Removal

### Task 1: Delete UploadHandler — it is never called

**Files:**
- Delete: `srcs/UploadHandler.cpp`
- Delete: `includes/UploadHandler.hpp`
- Modify: `srcs/ProcessRequest.cpp` (remove include)
- Modify: `Makefile` (remove from source list)

**Why:** `UploadHandler` has ~230 lines of upload logic that is never invoked from anywhere. `ProcessRequest` has its own inline upload path (`_handleUploadIfNeeded` / `_saveUpload`). Two dead systems in the same codebase is misleading.

- [ ] **Step 1: Confirm UploadHandler is unreachable**

```bash
grep -rn "UploadHandler" /home/pskpe/webserve/srcs/ /home/pskpe/webserve/includes/ /home/pskpe/webserve/main.cpp
```
Expected: only its own `.cpp`/`.hpp` definition lines appear — no call sites.

- [ ] **Step 2: Delete the files**

```bash
rm /home/pskpe/webserve/srcs/UploadHandler.cpp
rm /home/pskpe/webserve/includes/UploadHandler.hpp
```

- [ ] **Step 3: Remove include from ProcessRequest.cpp**

In `srcs/ProcessRequest.cpp`, remove line:
```cpp
#include "../includes/UploadHandler.hpp"
```

- [ ] **Step 4: Remove from Makefile**

Open `Makefile` and remove `srcs/UploadHandler.cpp` from whichever `SRCS` or file list it appears in.

- [ ] **Step 5: Build to verify clean compile**

```bash
cd /home/pskpe/webserve && make
```
Expected: no errors or warnings about missing UploadHandler.

---

### Task 2: Remove CgiConfig and stale declarations from Config.hpp

**Files:**
- Modify: `includes/config/Config.hpp`

**Why:** `CgiConfig` (lines 8–11) is defined but never instantiated. `createDefaultServerConfig()` and `createDefaultServerConfigs()` (lines 53–56) are declared but have no definitions — they're stubs that the config file parser made obsolete.

- [ ] **Step 1: Remove CgiConfig struct**

In `includes/config/Config.hpp`, delete:
```cpp
struct CgiConfig {
    std::string extension;   // e.g. ".py"
    std::string interpreter; // e.g. "/usr/bin/python3"
};
```

- [ ] **Step 2: Remove stale function declarations**

Delete these lines:
```cpp
// todo: placeholder to be replaced by filename.conf parser
ServerConfig createDefaultServerConfig();

// Returns all server blocks — swap this call for parseConfigFile() when parser is ready
std::vector<ServerConfig> createDefaultServerConfigs();
```

- [ ] **Step 3: Remove stale TODO comments**

Delete the comment on the `struct ServerConfig` line:
```cpp
// todo: hard coded values for now, to be replaced by filename.conf parser
```
And remove the inline comment on the `upload_enabled` field:
```cpp
// todo: is it used?? check all 3 for uploads if used
```

- [ ] **Step 4: Build to verify**

```bash
cd /home/pskpe/webserve && make
```
Expected: clean build. If any file referenced these declarations, the compiler will report it.

---

### Task 3: Remove Server::_running — it is never read

**Files:**
- Modify: `includes/Server.hpp`
- Modify: `srcs/Server.cpp`

**Why:** `_running` is set to `false` in `stop()` but the shutdown loop in `main.cpp` reads `g_running` (not `_running`). The field is pure dead state.

- [ ] **Step 1: Remove field from header**

In `includes/Server.hpp`, remove from the private section:
```cpp
bool                    _running;
```

- [ ] **Step 2: Remove initializer in Server.cpp constructor**

In `srcs/Server.cpp`, in the constructor initializer list, remove:
```cpp
_running(true),
```

- [ ] **Step 3: Remove the assignment in stop()**

In `srcs/Server.cpp::stop()`, remove:
```cpp
_running = false;
```
Keep the `std::cout` print — it's useful.

- [ ] **Step 4: Build to verify**

```bash
cd /home/pskpe/webserve && make
```

---

### Task 4: Fix .gitignore — stop tracking compiled artifacts

**Files:**
- Modify: `.gitignore`

**Why:** `srcs/*.o`, `obj/**/*.o`, and `main.o` are compiled object files that are committed to the repo. Build artifacts must not be tracked.

- [ ] **Step 1: Add patterns to .gitignore**

Add to `/home/pskpe/webserve/.gitignore`:
```
*.o
obj/
```

- [ ] **Step 2: Untrack already-committed .o files**

```bash
cd /home/pskpe/webserve
git rm --cached srcs/*.o srcs/config/*.o main.o obj/main.o 2>/dev/null
git rm -r --cached obj/ 2>/dev/null
true  # don't fail if files weren't tracked
```

- [ ] **Step 3: Verify clean build still works**

```bash
cd /home/pskpe/webserve && make clean && make
```
Expected: clean build from scratch.

---

### Task 5: Clean up duplicate/unnecessary includes in Server.hpp

**Files:**
- Modify: `includes/Server.hpp`

**Why:** `Server.hpp` pulls in `<sys/socket.h>`, `<arpa/inet.h>`, `<fcntl.h>` etc. at the header level. These are only needed in `Server.cpp`. Headers should only include what they directly use in their own declarations; the rest belong in the `.cpp`.

- [ ] **Step 1: Identify what Server.hpp actually uses in its declarations**

The header declares: `Server(const std::vector<ServerConfig>&)`, `void init()`, `void tick()`, `void stop()`. It uses: `std::map`, `std::vector`, `ServerConfig`, `EpollLoop`, `Client*`, `ProcessRequest`, `ConnectionManager`. Check which system headers are needed for the class definition itself vs. only in the implementation.

- [ ] **Step 2: Move implementation-only includes to Server.cpp**

In `includes/Server.hpp`, remove includes that are only used inside `.cpp` function bodies:
```cpp
// Remove these from the header (keep in Server.cpp):
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
```
Keep: `<map>`, `<vector>`, `<sys/epoll.h>` (needed for `epoll_event` in `tick()`), and all the local class includes.

- [ ] **Step 3: Ensure Server.cpp has all needed includes**

In `srcs/Server.cpp`, verify these are already present (add if missing):
```cpp
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.l>
#include <cstring>
```

- [ ] **Step 4: Build to verify**

```bash
cd /home/pskpe/webserve && make
```
Expected: clean build. If any other file relied on transitive includes from Server.hpp, the compiler will tell you.

---

### Task 6: Fix missing HTTP reason strings — 201, 204, 501, 504

**Files:**
- Modify: `srcs/HttpResponse.cpp`
- Modify: `includes/HttpResponse.hpp`

**Why:** `_reason()` returns `"Unknown"` for 201, 204, 501, 504. Clients and proxies see `HTTP/1.1 201 Unknown` which is wrong. Also add `make_501()` and `make_504()` static builders for use in later tasks.

- [ ] **Step 1: Add missing reason strings**

In `srcs/HttpResponse.cpp`, in `_reason()`, add before the `default` case:
```cpp
case 201: return "Created";
case 204: return "No Content";
case 303: return "See Other";
case 307: return "Temporary Redirect";
case 308: return "Permanent Redirect";
case 501: return "Not Implemented";
case 504: return "Gateway Timeout";
```
Check that 301/302 are already present; don't duplicate them.

- [ ] **Step 2: Add make_501() and make_504() in HttpResponse.cpp**

After `make_500()`:
```cpp
HttpResponse HttpResponse::make_501() {
    HttpResponse r;
    r.setStatus(501).setBody(_errorBody(501, "Not Implemented"));
    return r;
}

HttpResponse HttpResponse::make_504() {
    HttpResponse r;
    r.setStatus(504).setBody(_errorBody(504, "Gateway Timeout"));
    return r;
}
```

- [ ] **Step 3: Declare both in HttpResponse.hpp**

In `includes/HttpResponse.hpp`, alongside the other static builders, add:
```cpp
static HttpResponse make_501();
static HttpResponse make_504();
```

- [ ] **Step 4: Build and smoke-test**

```bash
cd /home/pskpe/webserve && make && ./webserv default.conf &
curl -sv http://localhost:8080/ 2>&1 | grep "< HTTP"
# Expected: HTTP/1.1 200 OK
kill %1
```

---

### Task 7: Fix unsupported HTTP methods — return 501 instead of 400

**Files:**
- Modify: `includes/HttpRequest.hpp`
- Modify: `srcs/HttpRequest.cpp`
- Modify: `srcs/ProcessRequest.cpp`

**Why:** HEAD, OPTIONS, PUT, PATCH, CONNECT, TRACE are valid HTTP methods. Currently `_parse_method` returns `false` for them → PARSE_ERROR → `ConnectionManager` sends 400 Bad Request. RFC 7231 §6.6.2 requires **501 Not Implemented** for recognised-but-unsupported methods.

- [ ] **Step 1: Add NOT_IMPLEMENTED to Method enum**

In `includes/HttpRequest.hpp`, update the enum:
```cpp
enum Method { GET, POST, DELETE, NOT_IMPLEMENTED, UNKNOWN };
```

- [ ] **Step 2: Update _parse_method in HttpRequest.cpp**

Replace the `else` branch:
```cpp
bool HttpRequest::_parse_method(const std::string &tok) {
    if      (tok == "GET")    method = GET;
    else if (tok == "POST")   method = POST;
    else if (tok == "DELETE") method = DELETE;
    else if (tok == "HEAD"    || tok == "OPTIONS" ||
             tok == "PUT"     || tok == "PATCH"   ||
             tok == "CONNECT" || tok == "TRACE")
        method = NOT_IMPLEMENTED;
    else { method = UNKNOWN; return false; }
    return true;
}
```

- [ ] **Step 3: Update debugPrint array in HttpRequest.cpp**

```cpp
const char *m[] = { "GET", "POST", "DELETE", "NOT_IMPLEMENTED", "UNKNOWN" };
```

- [ ] **Step 4: Handle NOT_IMPLEMENTED in ProcessRequest::handle**

In `srcs/ProcessRequest.cpp::handle()`, after the `UNKNOWN` check, add:
```cpp
if (req.method == NOT_IMPLEMENTED) {
    client.writeBuf = HttpResponse::make_501().serialize();
    client.keep_alive = false;
    stampConnection(client.writeBuf, false);
    return;
}
```

- [ ] **Step 5: Build and test**

```bash
cd /home/pskpe/webserve && make && ./webserv default.conf &
curl -sv -X OPTIONS http://localhost:8080/ 2>&1 | grep "< HTTP"
# Expected: HTTP/1.1 501 Not Implemented
curl -sv -X HEAD http://localhost:8080/ 2>&1 | grep "< HTTP"
# Expected: HTTP/1.1 501 Not Implemented
curl -sv http://localhost:8080/ 2>&1 | grep "< HTTP"
# Expected: HTTP/1.1 200 OK  (normal GET still works)
kill %1
```

---

### Task 8: Fix redirect — handle 303, 307, 308

**Files:**
- Modify: `srcs/ProcessRequest.cpp`

**Why:** `_handleRedirectIfNeeded` only fires for `redirect_code == 301 || == 302`. Any other 3xx code set in the config silently falls through to file serving instead of redirecting. Reason strings for 303/307/308 were added in Task 6.

- [ ] **Step 1: Widen the redirect condition**

In `srcs/ProcessRequest.cpp::_handleRedirectIfNeeded`, replace:
```cpp
if (loc.redirect_code == 301 || loc.redirect_code == 302) {
```
With:
```cpp
if (loc.redirect_code >= 300 && loc.redirect_code < 400) {
```

- [ ] **Step 2: Build and verify**

```bash
cd /home/pskpe/webserve && make
```

---

### Task 9: Move CgiExecutor file-scope helpers to anonymous namespace

**Files:**
- Modify: `srcs/CgiExecutor.cpp`

**Why:** `nowMs()` and `toString()` are free functions with global linkage and common names. This risks ODR collisions if another translation unit defines identically-named functions. Wrapping in an anonymous namespace restricts their linkage to this translation unit.

- [ ] **Step 1: Wrap both helpers in anonymous namespace**

In `srcs/CgiExecutor.cpp`, wrap the two helper functions:
```cpp
namespace {

long long nowMs() {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return (0);
    }
    return (static_cast<long long>(ts.tv_sec) * 1000LL
            + static_cast<long long>(ts.tv_nsec / 1000000LL));
}

std::string toString(std::size_t value) {
    std::ostringstream oss;
    oss << value;
    return (oss.str());
}

} // namespace
```

- [ ] **Step 2: Build to verify**

```bash
cd /home/pskpe/webserve && make
```

---

### Task 10: Fix config parser — overflow-safe toUnsigned + port range validation

**Files:**
- Modify: `srcs/config/ConfigParser.cpp`

**Why:** `Parser::toUnsigned` uses `istringstream >> unsigned long` which silently produces wrong values on overflow. A config with `listen 999999999999;` or `clientMaxBodySize 9999999999999999999;` would be silently accepted with a wrong value. Add `strtoul`-based overflow detection and validate port is in range 1–65535.

- [ ] **Step 1: Add cerrno include if missing**

At the top of `srcs/config/ConfigParser.cpp`, confirm or add:
```cpp
#include <cerrno>
#include <cstdlib>
```

- [ ] **Step 2: Replace toUnsigned implementation**

```cpp
unsigned long Parser::toUnsigned(const std::string& text) {
    char* end = nullptr;
    errno = 0;
    unsigned long value = std::strtoul(text.c_str(), &end, 10);
    if (errno == ERANGE || end == text.c_str() || *end != '\0') {
        return (static_cast<unsigned long>(-1)); // sentinel: overflow or bad parse
    }
    return (value);
}
```

- [ ] **Step 3: Add port range check in assignKnownServerFields**

In the `listen` directive block:
```cpp
if (key.value == "listen") {
    if (values.size() != 1 || !isUnsigned(values[0])) {
        throw ParseError("listen expects one numeric value", key.line, key.column);
    }
    unsigned long port = toUnsigned(values[0]);
    if (port == static_cast<unsigned long>(-1) || port < 1 || port > 65535) {
        throw ParseError("listen port must be 1–65535", key.line, key.column);
    }
    server.port = static_cast<int>(port);
}
```

- [ ] **Step 4: Add overflow check for clientMaxBodySize (server level)**

```cpp
} else if (key.value == "clientMaxBodySize") {
    if (values.size() != 1 || !isUnsigned(values[0])) {
        throw ParseError("clientMaxBodySize expects one numeric value", key.line, key.column);
    }
    unsigned long val = toUnsigned(values[0]);
    if (val == static_cast<unsigned long>(-1)) {
        throw ParseError("clientMaxBodySize value overflows", key.line, key.column);
    }
    server.clientMaxBodySize = static_cast<long>(val);
}
```

Apply the same overflow guard to the location-level `clientMaxBodySize` in `assignKnownLocationFields`.

- [ ] **Step 5: Test with out-of-range port**

```bash
cd /home/pskpe/webserve && make
printf 'server {\n  listen 99999;\n  host 127.0.0.1;\n  server_name test;\n  location / { root ./www; allowedMethod GET; }\n}\n' > /tmp/bad.conf
./webserv /tmp/bad.conf
# Expected: error message about port range, server exits with code 1
```

---

## Wave B — Bug Fixes

### Task 11: Fix use-after-free — Server::tick calls writeClient on deleted client

**Files:**
- Modify: `srcs/Server.cpp`

**Why:** In `tick()`:
```cpp
if (ev & EPOLLIN)  _connection_manager.readClient(client, READ_BUF);
if (ev & EPOLLOUT) _connection_manager.writeClient(client);
```
`readClient` may call `closeClient(fd)` on error, which deletes the `Client*` and erases it from `_clients`. If `ev` also had `EPOLLOUT` set, the second line calls `writeClient` on freed memory — undefined behaviour.

- [ ] **Step 1: Re-validate the fd exists before writeClient**

In `srcs/Server.cpp::tick()`, replace:
```cpp
if (ev & EPOLLIN)  _connection_manager.readClient(client, READ_BUF);
if (ev & EPOLLOUT) _connection_manager.writeClient(client);
```
With:
```cpp
if (ev & EPOLLIN)
    _connection_manager.readClient(client, READ_BUF);
// readClient may have closed fd — re-check before writing
if ((ev & EPOLLOUT) && _clients.count(fd))
    _connection_manager.writeClient(*_clients[fd]);
```

- [ ] **Step 2: Build and stress-test**

```bash
cd /home/pskpe/webserve && make && ./webserv default.conf &
# Rapid connect-and-drop (triggers the race between EPOLLIN close + EPOLLOUT)
for i in $(seq 1 30); do
    (echo -e "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n" | nc -q0 localhost 8080) &
done
wait
kill %1
```
Expected: no segfault, server stays running.

---

### Task 12: Fix double waitpid in CgiExecutor — potential hang

**Files:**
- Modify: `srcs/CgiExecutor.cpp`

**Why:** The read loop calls `waitpid(pid, &status, WNOHANG)` and may fully collect the child process (setting `childCollected`). After the loop, line ~266 calls `waitpid(pid, &status, 0)` unconditionally. If the child was already collected, this second call blocks forever waiting for a zombie that no longer exists.

- [ ] **Step 1: Add childCollected flag before the read loop**

In `CgiExecutor::execute`, just before `while (true) {`:
```cpp
bool childCollected = false;
```

- [ ] **Step 2: Set childCollected = true in the WNOHANG branch**

Find the block inside the loop:
```cpp
pid_t done = waitpid(pid, &status, WNOHANG);
if (done == pid) {
```
Add `childCollected = true;` as its first line:
```cpp
pid_t done = waitpid(pid, &status, WNOHANG);
if (done == pid) {
    childCollected = true;
    // ... rest of existing exit code logic unchanged ...
```

- [ ] **Step 3: Guard the final waitpid**

Replace the unconditional block after the loop (around line 264):
```cpp
int status = 0;
waitpid(pid, &status, 0);
if (result.exit_code < 0 && WIFEXITED(status)) {
    result.exit_code = WEXITSTATUS(status);
}
```
With:
```cpp
if (!childCollected) {
    int status2 = 0;
    waitpid(pid, &status2, 0);
    if (result.exit_code < 0 && WIFEXITED(status2)) {
        result.exit_code = WEXITSTATUS(status2);
    }
}
```

- [ ] **Step 4: Build and test CGI doesn't hang**

```bash
cd /home/pskpe/webserve && make && ./webserv tests/cgi_test.conf &
curl -sv --max-time 10 http://localhost:8080/cgi-bin/pythoncgitest.py
# Expected: response within seconds, not a 10-second timeout
kill %1
```

---

### Task 13: Fix unreachable 504 — check timed_out before success

**Files:**
- Modify: `srcs/ProcessRequest.cpp`
- Modify: `srcs/ErrorResponseBuilder.cpp`

**Why:** In `_executeCgiOrError`, `result.success` is `false` whenever `result.timed_out` is `true`. The current order checks `!result.success` first, so the `timed_out` check below it can never fire — timeouts always return 500 instead of 504.

- [ ] **Step 1: Reorder the checks**

In `srcs/ProcessRequest.cpp::_executeCgiOrError`, replace the existing two-check block:
```cpp
if (result.timed_out) {
    HttpResponse response;
    response.setStatus(504)
            .setBody("<html><body><h1>504 Gateway Timeout</h1>"
                     "<p>CGI script took too long</p></body></html>", "text/html");
    client.writeBuf = response.serialize();
    return true;
}

if (!result.success) {
    HttpResponse response;
    response.setStatus(500)
            .setBody("<html><body><h1>500 Internal Server Error</h1>"
                     "<p>CGI execution failed</p></body></html>", "text/html");
    client.writeBuf = response.serialize();
    return true;
}
```
(timed_out is now checked first.)

- [ ] **Step 2: Add case 504 to ErrorResponseBuilder**

In `srcs/ErrorResponseBuilder.cpp::_defaultErrorResponse`:
```cpp
case 504: return HttpResponse::make_504();
```

- [ ] **Step 3: Build to verify**

```bash
cd /home/pskpe/webserve && make
```

---

### Task 14: Fix blocking send in handleServerTimeout

**Files:**
- Modify: `srcs/Server.cpp`

**Why:** `handleServerTimeout()` calls `send(fd, ...)` directly on a non-blocking socket. If the client's receive buffer is full, `send` returns `EAGAIN` and the 408 response is silently lost. On a half-open connection it may also partially block. The correct approach is to queue the response in `writeBuf` and let the normal write path handle it.

- [ ] **Step 1: Replace direct send with writeBuf + epoll switch**

In `srcs/Server.cpp::handleServerTimeout()`, replace:
```cpp
std::string resp = ErrorResponseBuilder::buildErrorResponse(408, _configs[0]).serialize();
send(fd, resp.c_str(), resp.size(), 0);
++it;
_connection_manager.closeClient(fd);
```
With:
```cpp
client->writeBuf = ErrorResponseBuilder::buildErrorResponse(408, _configs[0]).serialize();
client->keep_alive = false;
_epoll.mod(fd, EPOLLOUT | EPOLLRDHUP);
++it;
```
The normal `writeClient` path will send the response and then close when `keep_alive` is false.

- [ ] **Step 2: Build and test timeout response is actually received**

```bash
cd /home/pskpe/webserve && make && ./webserv default.conf &
# Send a partial request then stall — wait longer than SERVER_TIMEOUT
(echo -ne "GET / HTTP/1.1\r\nHost: localhost\r\n"; sleep 15) | nc localhost 8080
# Expected: receives "HTTP/1.1 408 Request Timeout" after ~6 seconds
kill %1
```

---

### Task 15: Fix memory leak — servers not cleaned up when init() throws

**Files:**
- Modify: `main.cpp`

**Why:** If `s->init()` throws for server N, the `catch` block only prints and returns. Servers 0 through N-1 were already pushed to `servers` but their destructors (which close file descriptors) are never called.

- [ ] **Step 1: Add cleanup in the catch block**

In `main.cpp`, update the `catch`:
```cpp
} catch (const std::exception &e) {
    std::cerr << "Fatal: " << e.what() << "\n";
    for (size_t i = 0; i < servers.size(); ++i) {
        servers[i]->stop();
        delete servers[i];
    }
    return (1);
}
```

- [ ] **Step 2: Build to verify**

```bash
cd /home/pskpe/webserve && make
```

---

### Task 16: Fix inet_addr error not checked in Server::init

**Files:**
- Modify: `srcs/Server.cpp`

**Why:** `inet_addr()` returns `INADDR_NONE` (0xFFFFFFFF) on an invalid IP string. The result is used directly without checking, which silently binds to the wrong address or passes garbage to `bind()`.

- [ ] **Step 1: Add INADDR_NONE guard**

In `srcs/Server.cpp::init()`, replace:
```cpp
else
    addr.sin_addr.s_addr = inet_addr(_configs[0].host.c_str());
```
With:
```cpp
else {
    in_addr_t ip = inet_addr(_configs[0].host.c_str());
    if (ip == INADDR_NONE)
        throw std::runtime_error("invalid host address: " + _configs[0].host);
    addr.sin_addr.s_addr = ip;
}
```

- [ ] **Step 2: Test with an invalid IP**

```bash
cd /home/pskpe/webserve && make
printf 'server {\n  listen 8080;\n  host not.an.ip;\n  server_name test;\n  location / { root ./www; allowedMethod GET; }\n}\n' > /tmp/badhost.conf
./webserv /tmp/badhost.conf
# Expected: "Fatal: invalid host address: not.an.ip", exit 1
```

---

## Wave C — Security Hardening

### Task 17: Fix UploadHandler path traversal — replace string prefix with realpath

**Files:**
- Modify: `srcs/UploadHandler.cpp`

**Why:** Both `handleUpload` and `handleDelete` verify the target path with a string prefix compare:
```cpp
if (target_path.compare(0, upload_dir.size(), upload_dir) != 0) { ... }
```
This does not resolve symlinks or `..` segments. A symlinked upload directory or an unusual filename could escape the root. `ProcessRequest::_canonicalizeWithinRoot` already uses `realpath` correctly — the same defence belongs here.

Note: UploadHandler is currently dead code (not called by ProcessRequest). This fix prepares it for when the two upload paths are unified in Task 23.

- [ ] **Step 1: Add a canonicalize helper at the top of UploadHandler.cpp**

After the includes in `srcs/UploadHandler.cpp`, add:
```cpp
#include <climits>
#include <cstdlib>

static std::string canonicalWithin(const std::string& root, const std::string& path) {
    char rootBuf[PATH_MAX], pathBuf[PATH_MAX];
    if (!realpath(root.c_str(), rootBuf)) return "";
    if (!realpath(path.c_str(), pathBuf)) return "";
    std::string r(rootBuf), p(pathBuf);
    if (p.size() < r.size()) return "";
    if (p.compare(0, r.size(), r) != 0) return "";
    if (p.size() > r.size() && p[r.size()] != '/') return "";
    return p;
}
```

- [ ] **Step 2: Replace the traversal check in handleUpload**

Replace the `if (target_path.compare(0, upload_dir.size(), upload_dir) != 0)` block:
```cpp
std::string safeTarget = canonicalWithin(loc.upload_path, target_path);
if (safeTarget.empty()) {
    HttpResponse resp;
    resp.setStatus(403).setBody("Access denied", "text/plain");
    return (resp);
}
target_path = safeTarget;
```

- [ ] **Step 3: Same fix in handleDelete**

Apply the identical replacement to the traversal check in `handleDelete`.

- [ ] **Step 4: Build to verify**

```bash
cd /home/pskpe/webserve && make
```

---

### Task 18: Guard www/secrets from HTTP access

**Files:**
- Modify: `default.conf` (and all other `.conf` files that have a `/` root pointing at `www/`)

**Why:** `www/secrets/secret_pwd.txt` is inside the served web root. Without an explicit deny, any location that maps to `./www` exposes it.

- [ ] **Step 1: Check current exposure**

```bash
cd /home/pskpe/webserve && make && ./webserv default.conf &
curl -sv http://localhost:8080/secrets/secret_pwd.txt 2>&1 | grep "< HTTP"
kill %1
```
If the response is 200, the file is exposed and the fix is mandatory.

- [ ] **Step 2: Add deny_all in default.conf**

Inside the relevant `server` block in `default.conf`, add:
```nginx
location /secrets {
    deny all;
}
```

- [ ] **Step 3: Apply to all other .conf files**

Check `default_four.conf`, `long.conf`, `tests/cgi_test.conf` for the same exposure and add the same block where needed.

- [ ] **Step 4: Verify 403**

```bash
./webserv default.conf &
curl -sv http://localhost:8080/secrets/secret_pwd.txt 2>&1 | grep "< HTTP"
# Expected: HTTP/1.1 403 Forbidden
kill %1
```

---

### Task 19: Add upload_enabled and upload_allowed_types parsing + enforce file type

**Files:**
- Modify: `srcs/config/ConfigParser.cpp`
- Modify: `srcs/ProcessRequest.cpp`
- Modify: `srcs/ErrorResponseBuilder.cpp`

**Why:** `Location::upload_enabled` is never parsed — it is always `false`, silently disabling all uploads. `upload_allowed_types` is also unparsed and unchecked, allowing any file type to be uploaded. Uploading `.sh` or `.py` to a CGI-enabled directory would allow remote code execution.

- [ ] **Step 1: Add upload_enabled parsing in ConfigParser.cpp**

In `assignKnownLocationFields`, add:
```cpp
} else if (key.value == "upload_enabled") {
    if (values.size() != 1 || (values[0] != "on" && values[0] != "off")) {
        throw ParseError("upload_enabled expects one value: on|off", key.line, key.column);
    }
    location.upload_enabled = (values[0] == "on");
}
```

- [ ] **Step 2: Add upload_allowed_types parsing in ConfigParser.cpp**

```cpp
} else if (key.value == "upload_allowed_types") {
    if (values.empty()) {
        throw ParseError("upload_allowed_types expects at least one extension", key.line, key.column);
    }
    location.upload_allowed_types = values;
}
```

- [ ] **Step 3: Add file-type check in _handleUploadIfNeeded in ProcessRequest.cpp**

After `filename = normalizeUploadFilename(filename);` and the empty-check, add:
```cpp
if (!loc.upload_allowed_types.empty()) {
    size_t dot = filename.rfind('.');
    std::string ext = (dot == std::string::npos) ? "" : filename.substr(dot);
    bool typeOk = false;
    for (size_t i = 0; i < loc.upload_allowed_types.size(); ++i) {
        if (loc.upload_allowed_types[i] == ext) { typeOk = true; break; }
    }
    if (!typeOk) {
        client.writeBuf = ErrorResponseBuilder::buildErrorResponse(415, cfg).serialize();
        return true;
    }
}
```

- [ ] **Step 4: Add case 415 to ErrorResponseBuilder**

In `srcs/ErrorResponseBuilder.cpp::_defaultErrorResponse`:
```cpp
case 415: return HttpResponse::make_415();
```
(`make_415()` already exists in `HttpResponse.cpp`.)

- [ ] **Step 5: Build to verify**

```bash
cd /home/pskpe/webserve && make
```

---

### Task 20: Canonicalize index file path in _serveFromStat

**Files:**
- Modify: `srcs/ProcessRequest.cpp`

**Why:** In `_serveFromStat`, when a directory is requested and an index file exists:
```cpp
indexPath += loc.index;
HttpResponse indexResponse = StaticFileHandler::serveStatic(indexPath);
```
This path is passed directly to `serveStatic` without going through `_canonicalizeWithinRoot`. A symlinked directory tree could make `indexPath` point outside the server root.

- [ ] **Step 1: Canonicalize indexPath before stat and serve**

In `srcs/ProcessRequest.cpp::_serveFromStat`, replace:
```cpp
struct stat ist;
if (stat(indexPath.c_str(), &ist) == 0 && S_ISREG(ist.st_mode)) {
    HttpResponse indexResponse = StaticFileHandler::serveStatic(indexPath);
```
With:
```cpp
std::string safeIndex = _canonicalizeWithinRoot(loc.root, indexPath);
struct stat ist;
if (!safeIndex.empty() && stat(safeIndex.c_str(), &ist) == 0 && S_ISREG(ist.st_mode)) {
    HttpResponse indexResponse = StaticFileHandler::serveStatic(safeIndex);
```

- [ ] **Step 2: Build and verify index files still serve**

```bash
cd /home/pskpe/webserve && make && ./webserv default.conf &
curl -sv http://localhost:8080/ 2>&1 | grep "< HTTP"
# Expected: HTTP/1.1 200 OK
kill %1
```

---

## Wave D — Polish

### Task 21: Add missing MIME types

**Files:**
- Modify: `srcs/StaticFileHandler.cpp`

**Why:** Modern web files like `.svg`, `.webp`, `.mp4`, `.woff2` return `application/octet-stream`. Browsers display them as downloads instead of rendering them correctly.

- [ ] **Step 1: Add MIME entries before the default return**

In `srcs/StaticFileHandler.cpp::mimeType()`, before `return "application/octet-stream"`:
```cpp
if (ext == ".svg")  return "image/svg+xml";
if (ext == ".webp") return "image/webp";
if (ext == ".mp4")  return "video/mp4";
if (ext == ".webm") return "video/webm";
if (ext == ".mp3")  return "audio/mpeg";
if (ext == ".woff") return "font/woff";
if (ext == ".woff2")return "font/woff2";
if (ext == ".xml")  return "application/xml";
if (ext == ".zip")  return "application/zip";
if (ext == ".csv")  return "text/csv";
```

- [ ] **Step 2: Build and test**

```bash
cd /home/pskpe/webserve && make && ./webserv default.conf &
curl -sv http://localhost:8080/one/common.css 2>&1 | grep "Content-Type"
# Expected: Content-Type: text/css
kill %1
```

---

### Task 22: Fix SERVER_TIMEOUT default — 6 s → 60 s

**Files:**
- Modify: `includes/Server.hpp`

**Why:** `SERVER_TIMEOUT = 6` is documented as "for testing". Standard keep-alive timeout per RFC 7230 is 60–120 seconds. At 6 seconds, legitimate clients with slow networks are dropped prematurely.

- [ ] **Step 1: Update the constant**

In `includes/Server.hpp`, change:
```cpp
SERVER_TIMEOUT = 6
```
To:
```cpp
SERVER_TIMEOUT = 60
```

- [ ] **Step 2: Build to verify**

```bash
cd /home/pskpe/webserve && make
```

---

### Task 23: Reduce per-server tick latency — lower POLL_TIMEOUT

**Files:**
- Modify: `includes/Server.hpp`

**Why:** The main loop serialises N servers, each with a 100 ms `epoll_wait`. With 2 servers, a request to the second server experiences up to 200 ms added latency just from the poll cycle. Lowering to 10 ms reduces this to 20 ms with negligible CPU impact.

> The correct long-term fix is a single shared epoll across all servers, but that requires an architectural refactor. This is a quick improvement tracked for a future wave.

- [ ] **Step 1: Lower POLL_TIMEOUT**

In `includes/Server.hpp`:
```cpp
POLL_TIMEOUT = 10,  // ms — was 100 ms; reduces multi-server round-trip latency
```

- [ ] **Step 2: Build to verify**

```bash
cd /home/pskpe/webserve && make
```

---

## Wave A (continued) — A2 & A3 — Left Intentionally Last

### Task 24 (A2): Remove hardcoded upload path fallback

**Files:**
- Modify: `srcs/ProcessRequest.cpp`

**Why:** `_saveUpload` falls back to `"./www/uploads"` when `loc.upload_path` is empty. This silently enables uploads to a hardcoded path even when the config doesn't explicitly allow it. After Task 19 added `upload_enabled` and `upload_path` parsing, we can require both to be explicitly configured.

Prerequisite: Task 19 must be complete (`upload_enabled` parsing is in place).

- [ ] **Step 1: Remove the hardcoded fallback in _saveUpload**

In `srcs/ProcessRequest.cpp::_saveUpload`, replace:
```cpp
std::string baseDir = loc.upload_path.empty() ? "./www/uploads" : loc.upload_path;
```
With:
```cpp
if (loc.upload_path.empty()) return false;
std::string baseDir = loc.upload_path;
```

- [ ] **Step 2: Verify upload still works with explicit config**

Ensure `default.conf` (or the relevant `.conf`) has `upload_path` and `upload_enabled on` set on the upload location. Then:
```bash
cd /home/pskpe/webserve && make && ./webserv default.conf &
curl -sv -X POST http://localhost:8080/upload \
  -H "X-Filename: hello.txt" \
  -H "Content-Type: text/plain" \
  --data 'hello world'
# Expected: 201 Created (with upload_path and upload_enabled on configured)
kill %1
```

- [ ] **Step 3: Verify upload fails gracefully without upload_path**

Test with a location that has `upload_enabled on` but no `upload_path`:
```bash
# Expected: 500 Internal Server Error (saveUpload returns false → 500)
```

---

### Task 25 (A3): Deduplicate CGI response parsing in _buildHttpResponseFromCgiOutput

**Files:**
- Modify: `srcs/ProcessRequest.cpp`

**Why:** `_buildHttpResponseFromCgiOutput` (lines 578–665) has two near-identical ~40-line blocks — one for `\r\n\r\n`, one for `\n\n`. Extracting a shared header-parse helper eliminates ~80 lines of duplicated logic.

Also fix: both existing paths do `line.substr(colonPos + 2)` to get the header value, which assumes a space after `:`. If a CGI script sends `Status:200 OK` (no space), this silently skips the first character of the value.

- [ ] **Step 1: Add a static parseCgiHeaders helper in ProcessRequest.cpp**

Add before `_buildHttpResponseFromCgiOutput`:
```cpp
static void parseCgiHeaders(const std::string& section,
                             HttpResponse& response,
                             bool& statusSet) {
    std::istringstream iss(section);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos) continue;
        std::string key   = line.substr(0, colonPos);
        std::string value = line.substr(colonPos + 1);
        // trim leading OWS
        size_t s = value.find_first_not_of(" \t");
        value = (s == std::string::npos) ? "" : value.substr(s);
        if (key == "Status") {
            int code = std::atoi(value.c_str());
            if (code > 0) { response.setStatus(code); statusSet = true; }
        } else if (key != "Content-Length") {
            response.setHeader(key, value);
        }
    }
}
```

- [ ] **Step 2: Replace _buildHttpResponseFromCgiOutput with the unified version**

```cpp
bool ProcessRequest::_buildHttpResponseFromCgiOutput(const std::string &raw,
                                                     HttpResponse &response) const {
    std::string headerSection, body;

    size_t pos = raw.find("\r\n\r\n");
    if (pos != std::string::npos) {
        headerSection = raw.substr(0, pos);
        body          = raw.substr(pos + 4);
    } else {
        pos = raw.find("\n\n");
        if (pos == std::string::npos) return false;
        headerSection = raw.substr(0, pos);
        body          = raw.substr(pos + 2);
    }

    response.setBody(body, "text/html");
    bool statusSet = false;
    parseCgiHeaders(headerSection, response, statusSet);
    if (!statusSet) response.setStatus(200);
    return true;
}
```

- [ ] **Step 3: Build and test CGI end-to-end**

```bash
cd /home/pskpe/webserve && make && ./webserv tests/cgi_test.conf &
curl -sv http://localhost:8080/cgi-bin/pythoncgitest.py
# Expected: valid CGI HTML response, correct status code
curl -sv http://localhost:8080/cgi-bin/echo.py
# Expected: valid response
kill %1
```

---

## Self-Review Checklist

- [x] All brainstorm items covered: A1→A9, B (use-after-free, double waitpid, 504, blocking send, leak, inet_addr), C (path traversal, secrets, types, index canon), D (MIME, timeout, tick), A2/A3 last
- [x] No `git commit` steps anywhere — developer commits
- [x] No placeholder text (no TBD/TODO in task bodies)
- [x] `NOT_IMPLEMENTED` enum value used in Task 7 and defined in Task 7 — consistent
- [x] `make_501()` declared in Task 6 and used in Task 7 — consistent
- [x] `make_504()` declared in Task 6 and used in Task 13 — consistent
- [x] `upload_enabled` parsed in Task 19 and relied on in Task 24 — Task 24 notes prerequisite
- [x] `parseCgiHeaders` defined and used in the same task (25) — no cross-task type mismatch
