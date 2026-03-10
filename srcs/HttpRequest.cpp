// HttpRequest.cpp
#include "../includes/HttpRequest.hpp"
#include <iostream>
#include <algorithm>
#include <stdexcept>

bool HttpRequest::has_header(const std::string &key) const {
    std::string lower = key;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return headers.count(lower) > 0;
}

std::string HttpRequest::get_header(const std::string &key) const {
    std::string lower = key;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    auto it = headers.find(lower);
    if (it == headers.end()) return "";
    return it->second;
}

size_t HttpRequest::content_length() const {
    std::string val = get_header("content-length");
    if (val.empty()) return 0;
    return static_cast<size_t>(std::stoul(val));
}

bool HttpRequest::is_valid() const {
    return method != UNKNOWN && !path.empty() && !version.empty();
}

void HttpRequest::clear() {
    method = UNKNOWN;
    path.clear();
    query_string.clear();
    version.clear();
    headers.clear();
    body.clear();
}

void HttpRequest::print() const {
    std::string methods[] = {"GET", "POST", "DELETE", "UNKNOWN"};
    std::cout << "=== HttpRequest ===\n";
    std::cout << "Method  : " << methods[method] << "\n";
    std::cout << "Path    : " << path << "\n";
    std::cout << "Query   : " << query_string << "\n";
    std::cout << "Version : " << version << "\n";
    std::cout << "Headers :\n";
    for (auto &[k, v] : headers)
        std::cout << "  " << k << ": " << v << "\n";
    if (!body.empty())
        std::cout << "Body    : " << body << "\n";
    std::cout << "===================\n";
}