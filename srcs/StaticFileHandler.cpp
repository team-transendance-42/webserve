#include "../includes/StaticFileHandler.hpp"

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

	!!NB: stat + access + open is a classic TOCTOU pattern (time-of-check vs time-of-use)
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

    if (S_ISDIR(st.st_mode)) // test in which case i really need it
        return HttpResponse::make_403();

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
    return "application/octet-stream"; // Default binary MIME type for unknown file extensions.
}

/**
 * DIR is an opaque struct type from <dirent.h>.
	It represents an opened directory stream (handle).
	You don’t create it directly; OS functions do.

	opendir()Opens a directory for iteration.

		struct dirent
	POSIX struct type from <dirent.h>.
	Represents one item in a directory listing.
	Common field you use: entry->d_name (file/folder name).
	readdir(dir): Reads next entry from the opened directory stream.
	Returns:
	pointer to dirent when an entry exists
	NULL when end reached or on error

	Why closedir(dir) is needed
	opendir() allocates OS resources (directory handle/file descriptor).
	C++ will not auto-close this C/POSIX handle unless you explicitly call closedir().
	Not calling it leaks resources.

*/

HttpResponse StaticFileHandler::autoindex(const std::string &dirpath,
                                          const std::string &url_path) {
    DIR *dir = opendir(dirpath.c_str()); // returns NULL on failure, sets errno
    if (!dir) return HttpResponse::make_403(); // return 403 because this func is called after server already confirms this path is dir!

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
    html << "</pre><hr></body></html>"; // pre = preformatted-text element. Preserves spaces and line breaks exactly as written.

    return HttpResponse::make_200(html.str());
}
