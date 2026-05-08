#include "../includes/StaticFileHandler.hpp"

#include <iostream>
#include <cerrno>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

/**
 * stat():
	Checks whether the path exists and is valid.
	Tries to access metadata (not open file content).
	Fills st with info if successful.
	It does not open/read the file body, it only gets file info.
	file metadata is (type, size, permissions, timestamps)
 */

HttpResponse StaticFileHandler::serveStatic(const std::string &filepath) {
    struct stat st; // POSIX API structure (size, mode/type, timestamps, etc.).
    if (stat(filepath.c_str(), &st) != 0) { //0 = success, -1 = failure and sets errno. 
        if (errno == ENOENT || errno == ENOTDIR) // error no entry(path or file doesnt exist, error no dir(/www/index.html/abc))
            return HttpResponse::make_404();
        if (errno == EACCES) // error access denied
            return HttpResponse::make_403();
        return HttpResponse::make_500();
    }

    if (S_ISDIR(st.st_mode))
        return HttpResponse::make_403(); // server refuses to serve the directory as a file

    // Ensure permission-denied on file read maps to 403, not generic 500.
    if (access(filepath.c_str(), R_OK) != 0) {
        if (errno == EACCES)
            return HttpResponse::make_403();
        return HttpResponse::make_500();
    }

    std::ifstream file(filepath.c_str(), std::ios::binary); //Open the file at filepath in binary mode.
    if (!file.is_open())
        return HttpResponse::make_500();

    std::ostringstream ss;
    ss << file.rdbuf(); // read buffer(read whole file into a string stream)
    return HttpResponse::make_200(ss.str(), mimeType(filepath));
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
    return "application/octet-stream"; // Default binary MIME type for unknown file extensions.
}

/**
 prevents HTML injection and broken markup by converting special characters (like <, >, &, ") into safe HTML entities.
 Example: "<script>" becomes "&lt;script&gt;"
 */
static std::string htmlEscape(const std::string &s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        switch (s[i]) {
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '&':  out += "&amp;";  break;
            case '"':  out += "&quot;"; break;
            default:   out += s[i];     break;
        }
    }
    return out;
}

/*
 special characters (like spaces, #, ?, &, non-ASCII) are safely included in URLs by converting them to percent-encoded form (like %20, %23, %3F, %26).
*/
static std::string urlEncode(const std::string &s) {
    std::ostringstream out;
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = s[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out << c;
        } else {
            out << '%' << std::uppercase << std::hex << (int)c << std::nouppercase << std::dec;
        }
    }
    return out.str();
}

/**
 *  DIR is an opaque struct type from <dirent.h>.
 *  Represents an opened directory stream (handle).
 *  opendir(path) → returns DIR* (directory handle)
 *  readdir(DIR*) → returns struct dirent* (info about each entry)
 *  You use these functions to list the files/folders inside a directory.
 *
 *  Why closedir(dir) is needed:
 *  opendir() allocates OS resources (directory handle/file descriptor).
 *  C++ will not auto-close this C/POSIX handle unless you explicitly call closedir().
 *  Not calling it leaks resources.
 */
HttpResponse StaticFileHandler::autoindex(const std::string &dirpath,
                                          const std::string &url_path) {
    DIR *dir = opendir(dirpath.c_str());
    if (!dir) return HttpResponse::make_403();

    std::ostringstream html;
    html << "<html><head><title>Index of " << htmlEscape(url_path) << "</title></head>"
         << "<body><h1>Index of " << htmlEscape(url_path) << "</h1><hr><pre>";

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        std::string name = entry->d_name;
        if (name == ".") continue;
        std::string safeName = htmlEscape(name);
        html << "<a href=\"" << url_path;
        if (url_path[url_path.size() - 1] != '/') html << '/';
        html << urlEncode(name) << "\">" << safeName << "</a>\n";
    }
    closedir(dir);
    html << "</pre><hr></body></html>";

    return HttpResponse::make_200(html.str());
}
