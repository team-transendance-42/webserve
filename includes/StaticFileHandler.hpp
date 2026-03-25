
#pragma once

#include <string>
#include "HttpResponse.hpp"

class StaticFileHandler {
public:
    static HttpResponse serveStatic(const std::string &filepath);
    static HttpResponse autoindex(const std::string &dirpath,
                                  const std::string &url_path);

private:
    static std::string mimeType(const std::string &path);
};
