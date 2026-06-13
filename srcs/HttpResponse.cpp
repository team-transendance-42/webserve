#include "../includes/HttpResponse.hpp"
#include <ctime>
#include <sstream>

// headers and body are default-constructed empty by std::map and std::string automatically.
HttpResponse::HttpResponse() : statusCode(200) {}

// ── setters ──────────────────────────────────────────────────────────

HttpResponse &HttpResponse::setStatus(int code) {
    statusCode = code;
    return *this; // this is a pointer to the current object, *this is the object itself. Returning *this allows method chaining, e.g., resp.setStatus(404).setBody("Not found");
}

HttpResponse &HttpResponse::setHeader(const std::string &key, const std::string &value) {
    headers[key] = value;
    return *this;
}

HttpResponse &HttpResponse::setBody(const std::string &content, const std::string &type) {
    body = content;
    headers["Content-Type"]   = type;
    headers["Content-Length"] = std::to_string(content.size());
    return *this;
}

// ── static builders ───────────────────────────────────────────────────────────

HttpResponse HttpResponse::make_200(const std::string &body,
                                    const std::string &type) {
    HttpResponse r;
    r.setStatus(200).setBody(body, type);
    return r;
}

HttpResponse HttpResponse::make_redirect(int code, const std::string &location) {
    HttpResponse r;
    int redirectCode = (code == 301) ? 301 : 302;
    r.setStatus(redirectCode);
    r.setHeader("Location", location);
    r.setHeader("Content-Length", "0");
    return r;
}

HttpResponse HttpResponse::make_err_page(const std::string &msg, int code) {
    HttpResponse r;
    r.setStatus(code).setBody(_errorBody(code, msg));
    return r;
}

/**
 *  ── serialize — builds full HTTP/1.1 message for send() ─────────────────
 Format produced:
   1) Status line:   "HTTP/1.1 <code> <reason>\r\n"
   2) Header lines:  "Key: Value\r\n" for each header map entry
   3) Header/body separator: "\r\n"
   4) Body bytes appended as-is
 The returned string is a complete response payload ready for socket send().

 * Serialized = converting structured data(like obj) into a flat byte/text format that can be sent or stored. HttpResponse is an object (status, headers, body)
 * .serialize() turns it into raw HTTP text
 */
void HttpResponse::injectConnectionHeader(std::string &response, bool keepAlive) {
    size_t pos = response.find("\r\n");
    if (pos == std::string::npos) return;
    const std::string header = keepAlive ? "Connection: keep-alive\r\n"
                                         : "Connection: close\r\n";
    response.insert(pos + 2, header);
}

std::string HttpResponse::serialize() const {
    std::string reason = _reason(statusCode);

    std::ostringstream oss;
    oss << "HTTP/1.1 " << statusCode << " " << reason << "\r\n";

    if (headers.find("Date") == headers.end()) {
        char datebuf[64];
        time_t now = time(nullptr);
        struct tm *gmt = gmtime(&now);
        strftime(datebuf, sizeof(datebuf), "%a, %d %b %Y %H:%M:%S GMT", gmt);
        oss << "Date: " << datebuf << "\r\n";
    }

    for (std::map<std::string,std::string>::const_iterator it = headers.begin();
         it != headers.end(); ++it)
        oss << it->first << ": " << it->second << "\r\n";

    oss << "\r\n" << body;
    return oss.str();
}

// ── private helpers ───────────────────────────────────────────────────────────

/* Maps a numeric HTTP status code to its standard reason phrase.
   The phrase appears in the status line: "HTTP/1.1 404 Not Found".
   RFC 9110 §8.1: the phrase is informational only — clients MUST use the
   numeric code for logic, not the text. But returning "Unknown" for codes
   the server actually sends signals a broken implementation to evaluators. */
std::string HttpResponse::_reason(int code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";           // resource was created (e.g. file upload)
        case 204: return "No Content";        // success with no body (e.g. DELETE)
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 400: return "Bad Request";
        case 409: return "Conflict";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 408: return "Request Timeout";
        case 413: return "Payload Too Large";
        case 415: return "Unsupported Media Type";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";   // valid method the server never supports (e.g. PATCH)
        case 504: return "Gateway Timeout";   // CGI script did not respond in time
        default:  return "Unknown";
    }
}

/* returns the HTML body for an error response */
std::string HttpResponse::_errorBody(int code, const std::string &reason) {
    std::ostringstream oss;
    oss << "<html><body><h1>"
        << code << " " << reason
        << "</h1></body></html>";
    return oss.str();
}
