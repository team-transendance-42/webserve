
#pragma once

#include <string>
#include "HttpResponse.hpp"

class StaticFileHandler {
public:
    static HttpResponse serveStatic(const std::string &filepath);
    static HttpResponse autoindex(const std::string &dirpath, const std::string &url_path);

private:
/** MIME(Multipurpose Internet Mail Extensions type) refers to a standardized way to indicate the nature and format of a file. For example:

For an HTML file: text/html
For a PNG image: image/png
For a CSS file: text/css */
    static std::string mimeType(const std::string &path);
};
