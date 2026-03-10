#pragma once
#include <string>
#include <map>

enum Method { GET, POST, DELETE, UNKNOWN };

class HttpRequest {
public:
    Method                              method;
    std::string                         path;
    std::string                         query_string;
    std::string                         version;
    std::map<std::string, std::string>  headers;
    std::string                         body;

    // helpers
    bool        has_header(const std::string &key) const;
    std::string get_header(const std::string &key) const;
    size_t      content_length() const;
    bool        is_valid() const;

    void        clear();
    void        print() const;   // debug
};