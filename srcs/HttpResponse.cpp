#include "../includes/HttpResponse.hpp"
#include <sstream>

HttpResponse::HttpResponse() : status_code(200) {}

// ── chaining setters ──────────────────────────────────────────────────────────

HttpResponse &HttpResponse::set_status(int code) {
    status_code = code;
    return *this;
}

HttpResponse &HttpResponse::set_header(const std::string &key,
                                       const std::string &value) {
    headers[key] = value;
    return *this;
}

HttpResponse &HttpResponse::set_body(const std::string &content,
                                     const std::string &type) {
    body = content;
    headers["Content-Type"]   = type;
    headers["Content-Length"] = _itoa(static_cast<int>(content.size()));
    return *this;
}

// ── static builders ───────────────────────────────────────────────────────────

HttpResponse HttpResponse::make_200(const std::string &body,
                                    const std::string &type) {
    HttpResponse r;
    r.set_status(200).set_body(body, type);
    return r;
}

HttpResponse HttpResponse::make_301(const std::string &location) {
    HttpResponse r;
    r.set_status(301);
    r.set_header("Location", location);
    r.set_body(_error_body(301, "Moved Permanently"));
    return r;
}

HttpResponse HttpResponse::make_302(const std::string &location) {
    HttpResponse r;
    r.set_status(302);
    r.set_header("Location", location);
    r.set_body(_error_body(302, "Found"));
    return r;
}

HttpResponse HttpResponse::make_400() {
    HttpResponse r;
    r.set_status(400).set_body(_error_body(400, "Bad Request"));
    return r;
}

HttpResponse HttpResponse::make_403() {
    HttpResponse r;
    r.set_status(403).set_body(_error_body(403, "Forbidden"));
    return r;
}

HttpResponse HttpResponse::make_404() {
    HttpResponse r;
    r.set_status(404).set_body(_error_body(404, "Not Found"));
    return r;
}

HttpResponse HttpResponse::make_405() {
    HttpResponse r;
    r.set_status(405).set_body(_error_body(405, "Method Not Allowed"));
    return r;
}

HttpResponse HttpResponse::make_500() {
    HttpResponse r;
    r.set_status(500).set_body(_error_body(500, "Internal Server Error"));
    return r;
}

// ── serialize — builds the raw string you hand to send() ─────────────────────

std::string HttpResponse::serialize() const {
    std::string reason = _reason(status_code);

    std::ostringstream oss;
    oss << "HTTP/1.1 " << status_code << " " << reason << "\r\n";

    for (std::map<std::string,std::string>::const_iterator it = headers.begin();
         it != headers.end(); ++it)
        oss << it->first << ": " << it->second << "\r\n";

    oss << "\r\n" << body;
    return oss.str();
}

// ── private helpers ───────────────────────────────────────────────────────────

std::string HttpResponse::_reason(int code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 304: return "Not Modified";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Payload Too Large";
        case 500: return "Internal Server Error";
        case 502: return "Bad Gateway";
        case 504: return "Gateway Timeout";
        default:  return "Unknown";
    }
}

std::string HttpResponse::_itoa(int n) {
    std::ostringstream oss;
    oss << n;
    return oss.str();
}

std::string HttpResponse::_error_body(int code, const std::string &reason) {
    std::ostringstream oss;
    oss << "<html><body><h1>"
        << code << " " << reason
        << "</h1></body></html>";
    return oss.str();
}

HttpResponse HttpResponse::make_413() {
    HttpResponse r;
    r.set_status(413).set_body(_error_body(413, "Payload Too Large"));
    return r;
}