#pragma once
#include <string>
#include <map>

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
    static HttpResponse make_301(const std::string &location);
    static HttpResponse make_302(const std::string &location);
    static HttpResponse make_400();
    static HttpResponse make_403();
    static HttpResponse make_404();
    static HttpResponse make_405();
	static HttpResponse make_413(); 
    static HttpResponse make_500();

    // serialize to raw HTTP string — hand this to send()
    std::string serialize() const;

private:
    static std::string _reason(int code);
    static std::string _itoa  (int n);
    static std::string _error_body(int code, const std::string &reason);
};