#include "../includes/StaticFileHandler.hpp"

#include <dirent.h>
#include <fstream>
#include <sstream>

HttpResponse StaticFileHandler::serveStatic(const std::string &filepath) {
    std::ifstream file(filepath.c_str(), std::ios::binary);
    if (!file.is_open())
        return HttpResponse::make_404();

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();

    return HttpResponse::make_200(content, mimeType(filepath));
}

std::string StaticFileHandler::mimeType(const std::string &path) {
    size_t dot = path.rfind('.');
    if (dot == std::string::npos) return "application/octet-stream";

    std::string ext = path.substr(dot);
    if (ext == ".html" || ext == ".htm") return "text/html";
    if (ext == ".css")                   return "text/css";
    if (ext == ".js")                    return "application/javascript";
    if (ext == ".json")                  return "application/json";
    if (ext == ".png")                   return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif")                   return "image/gif";
    if (ext == ".ico")                   return "image/x-icon";
    if (ext == ".txt")                   return "text/plain";
    if (ext == ".pdf")                   return "application/pdf";
    return "application/octet-stream";
}

HttpResponse StaticFileHandler::autoindex(const std::string &dirpath,
                                          const std::string &url_path) {
    DIR *dir = opendir(dirpath.c_str());
    if (!dir) return HttpResponse::make_403();

    std::ostringstream html;
    html << "<html><head><title>Index of " << url_path << "</title></head>"
         << "<body><h1>Index of " << url_path << "</h1><hr><pre>";

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        std::string name = entry->d_name;
        if (name == ".") continue;
        html << "<a href=\"" << url_path;
        if (url_path[url_path.size() - 1] != '/') html << '/';
        html << name << "\">" << name << "</a>\n";
    }
    closedir(dir);
    html << "</pre><hr></body></html>";

    return HttpResponse::make_200(html.str());
}
