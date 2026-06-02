#include "../includes/UploadHandler.hpp"
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <sys/stat.h>
#include <cerrno>
#include <unistd.h>

HttpResponse UploadHandler::handleUpload(const HttpRequest &req,
                                         const Location &loc) {
    // Check Content-Type: only accept raw binary uploads (not multipart)
    std::string content_type = req.getHeader("content-type");
    if (!content_type.empty() && content_type.find("multipart") != std::string::npos) {
        // Multipart form not supported here
        HttpResponse resp;
        resp.setStatus(415).setBody("Multipart uploads not supported", "text/plain");
        return (resp);
    }

    // Get filename from X-Filename header
    std::string filename = req.getHeader("x-filename");
    if (filename.empty()) {
        HttpResponse resp;
        resp.setStatus(400).setBody("Missing X-Filename header", "text/plain");
        return (resp);
    }

    // Validate filename
    if (!_isValidFilename(filename)) {
        HttpResponse resp;
        resp.setStatus(400).setBody("Invalid filename", "text/plain");
        return (resp);
    }

    // Ensure upload directory exists
    if (!_ensureUploadDir(loc.upload_path)) {
        HttpResponse resp;
        resp.setStatus(500).setBody("Failed to create upload directory", "text/plain");
        return (resp);
    }

    // Build target path: upload_path / filename
    std::string target_path = loc.upload_path;
    if (target_path[target_path.size() - 1] != '/') target_path += '/';
    target_path += filename;

    // Verify path stays inside upload_path (prevent /../../ attacks)
    // Rough check: target should start with loc.upload_path (with trailing /)
    std::string upload_dir = loc.upload_path;
    if (upload_dir[upload_dir.size() - 1] != '/') upload_dir += '/';
    if (target_path.compare(0, upload_dir.size(), upload_dir) != 0) {
        HttpResponse resp;
        resp.setStatus(403).setBody("Access denied", "text/plain");
        return (resp);
    }

    // Write body to file atomically
    if (!_writeFileAtomically(target_path, req.body)) {
        HttpResponse resp;
        resp.setStatus(500).setBody("Failed to write file", "text/plain");
        return (resp);
    }

    // Return 201 Created
    HttpResponse resp;
    resp.setStatus(201);
    std::ostringstream oss;
    oss << "File uploaded: " << filename;
    resp.setBody(oss.str(), "text/plain");
    return (resp);
}

HttpResponse UploadHandler::handleDelete(const HttpRequest &req,
                                         const Location &loc) {
    // Extract filename from path relative to location
    std::string filename = _extractFilenameFromPath(req.path, loc.path);
    if (filename.empty()) {
        HttpResponse resp;
        resp.setStatus(400).setBody("Invalid path", "text/plain");
        return (resp);
    }

    // Validate filename
    if (!_isValidFilename(filename)) {
        HttpResponse resp;
        resp.setStatus(403).setBody("Access denied", "text/plain");
        return (resp);
    }

    // Build target path
    std::string target_path = loc.upload_path;
    if (target_path[target_path.size() - 1] != '/') target_path += '/';
    target_path += filename;

    // Verify path stays inside upload_path
    std::string upload_dir = loc.upload_path;
    if (upload_dir[upload_dir.size() - 1] != '/') upload_dir += '/';
    if (target_path.compare(0, upload_dir.size(), upload_dir) != 0) {
        HttpResponse resp;
        resp.setStatus(403).setBody("Access denied", "text/plain");
        return (resp);
    }

    // Check file exists
    if (access(target_path.c_str(), F_OK) != 0) {
        HttpResponse resp;
        resp.setStatus(404).setBody("File not found", "text/plain");
        return (resp);
    }

    // Delete file
    if (unlink(target_path.c_str()) != 0) {
        HttpResponse resp;
        resp.setStatus(500).setBody("Failed to delete file", "text/plain");
        return (resp);
    }

    // Return 204 No Content
    HttpResponse resp;
    resp.setStatus(204);
    return (resp);
}

bool UploadHandler::_isValidFilename(const std::string &filename) {
    // Empty filename
    if (filename.empty())
		return (false);

    // Contains path separators
    if (filename.find('/') != std::string::npos ||
        filename.find('\\') != std::string::npos) {
        return (false);
    }

    // Contains ".." (directory traversal)
    if (filename.find("..") != std::string::npos)
		return (false);

    // Reject names starting with dot (hidden/system files)
    if (filename[0] == '.')
		return (false);

    return (true);
}

std::string UploadHandler::_extractFilenameFromPath(const std::string &req_path,
                                                    const std::string &loc_path) {
    // If request path == location path, no filename
    if (req_path == loc_path)
		return ("");

    // If request path doesn't start with location path, invalid
    if (req_path.compare(0, loc_path.size(), loc_path) != 0) {
        return ("");
    }

    // Extract relative part after location path
    size_t start = loc_path.size();
    if (start < req_path.size() && req_path[start] == '/') {
        start++;  // Skip leading slash
    }

    std::string relative = req_path.substr(start);
    if (relative.empty())
		return ("");

    // Return everything after location path (just filename for MVP)
    return (relative);
}

bool UploadHandler::_ensureUploadDir(const std::string &upload_path) {
    struct stat st;
    if (stat(upload_path.c_str(), &st) == 0) {
        // Path exists
        if (S_ISDIR(st.st_mode)) {
            return (true);  // Already a directory
        }
        return (false);  // Exists but not a directory
    }

    // Directory doesn't exist, try to create it
    // mkdir with 0755 permissions
    if (mkdir(upload_path.c_str(), 0755) == 0) {
        return (true);
    }

    // Check if errno is EEXIST (another process created it race condition)
    if (errno == EEXIST) {
        // Verify it's a directory
        if (stat(upload_path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
            return (true);
        }
    }

    return (false);
}

bool UploadHandler::_writeFileAtomically(const std::string &target_path,
                                         const std::string &body) {
    // Write to temporary file first
    std::string temp_path = target_path + ".tmp";

    // Remove any stale temp file
    unlink(temp_path.c_str());

    // Open and write temp file
    std::ofstream file(temp_path.c_str(), std::ios::binary);
    if (!file.is_open()) {
        return (false);
    }

    file.write(body.c_str(), static_cast<std::streamsize>(body.size()));
    if (!file) {
        file.close();
        unlink(temp_path.c_str());
        return (false);
    }
    file.close();

    // Rename temp to target (atomic on POSIX)
    if (rename(temp_path.c_str(), target_path.c_str()) != 0) {
        unlink(temp_path.c_str());
        return (false);
    }

    return (true);
}
