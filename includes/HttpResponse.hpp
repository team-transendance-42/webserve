#pragma once

#include <string>
#include <map>

/**
 *  no need to for full canonical form:
 *  It owns only STL types (std::string, std::map) and an int.
	No raw pointer ownership, no custom resource management.
	Compiler-generated copy constructor / assignment / destructor (Rule of Zero).

 */
 /**
 * HTTP response to be sent to the client
 * Stores status code, headers, and body content
 * Provides setters for building responses and static helpers for common status codes (200, 404, etc)
 * Used by the server to serialize(convert resp. obj into a sibnel raw string) and send HTTP responses after request handling.
 */
 
class HttpResponse {
public:
    int                                statusCode;
    std::map<std::string, std::string> headers;
    std::string                        body;

    HttpResponse();

    // setters — return *this for chaining
    HttpResponse &setStatus(int code);
    HttpResponse &setBody  (const std::string &content,
                             const std::string &contentType = "text/html");
    HttpResponse &setHeader(const std::string &key,
                             const std::string &value);

    // static builders — common responses in one line
    static HttpResponse make_200(const std::string &body,
                                 const std::string &type = "text/html");
    static HttpResponse make_redirect(int code, const std::string &location); // no html body, just headers
    static HttpResponse make_400(); // bad request: todo: add testing scenarios that trigger 400
    static HttpResponse make_403(); // forbidden: server knows who you are (or auth is irrelevant), but access is denied.
    static HttpResponse make_404(); // not found
    static HttpResponse make_405(); // method not allowed
    static HttpResponse make_408(); // request timeout: client idle for too long, server closes connection
    static HttpResponse make_413(); // payload too large
    static HttpResponse make_415(); // unsupported media type
    static HttpResponse make_500();

    // serialize(from http res obj) to raw HTTP string — hand this to send()
    std::string serialize() const;

private:
    static std::string _reason(int code);
    static std::string _errorBody(int code, const std::string &reason);
};