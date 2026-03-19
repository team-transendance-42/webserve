#pragma once

#include <string>
#include <map>

/**
 *  no need to for full canonical form:
 *  It owns only STL types (std::string, std::map) and an int.
	No raw pointer ownership, no custom resource management.
	Compiler-generated copy constructor / assignment / destructor are correct here (Rule of Zero).

 */
class HttpResponse {
public:
    int                                status_code;
    std::map<std::string, std::string> headers;
    std::string                        body;

    HttpResponse();

    // convenience setters — return *this for chaining
    HttpResponse &set_status(int code);
    HttpResponse &set_body  (const std::string &content,
                             const std::string &content_type = "text/html");
    HttpResponse &set_header(const std::string &key,
                             const std::string &value);

    // static builders — common responses in one line
    static HttpResponse make_200(const std::string &body,
                                 const std::string &type = "text/html");
    static HttpResponse make_301(const std::string &location); // moved permanantly:Typical use: old route replaced forever.
    static HttpResponse make_302(const std::string &location); // found, temp redirect
    static HttpResponse make_400(); // bad request
	//static HttpResponse make_401(); // 401 unauthorized: Client can retry with valid credentials: not implemented, do we need it?
    static HttpResponse make_403(); // forbidden: server knows who you are (or auth is irrelevant), but access is denied.
    static HttpResponse make_404(); // not found
    static HttpResponse make_405(); // method not allowed
    static HttpResponse make_413(); // payload too large
    static HttpResponse make_500();

    // serialize to raw HTTP string — hand this to send()
    std::string serialize() const;

private:
    static std::string _reason(int code);
    static std::string _error_body(int code, const std::string &reason);
};