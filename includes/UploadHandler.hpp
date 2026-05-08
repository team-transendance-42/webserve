#pragma once

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "config/Config.hpp"
#include <string>

/**
 * Handles raw body file uploads and deletions.
 * MVP supports:
 * - POST with X-Filename header: write body to file
 * - DELETE: remove a file from upload_path
 */
class UploadHandler {
public:
    /**
     * Handle POST upload: write request body to file under upload_path.
     * Requires X-Filename header; filename must not contain path separators.
     * Returns: 201 Created on success, 400/403/415 on validation failure.
     */
    static HttpResponse handleUpload(const HttpRequest &req,
                                     const Location &loc);

    /**
     * Handle DELETE: remove a file from upload_path.
     * Extracts filename from request path.
     * Returns: 204 No Content on success, 400/403/404 on failure.
     */
    static HttpResponse handleDelete(const HttpRequest &req,
                                     const Location &loc);

private:
    /**
     * Validate filename: no path separators, no "..", empty check.
     * Returns true if safe, false otherwise.
     */
    static bool _isValidFilename(const std::string &filename);

    /**
     * Extract the filename from the request path relative to location.
     * Example: req.path="/upload/myfile.txt", loc.path="/upload" => "myfile.txt"
     */
    static std::string _extractFilenameFromPath(const std::string &req_path,
                                                const std::string &loc_path);

    /**
     * Ensure upload directory exists; create if needed.
     * Returns true on success or if already exists, false on creation failure.
     */
    static bool _ensureUploadDir(const std::string &upload_path);

    /**
     * Safely write body to file using temp+rename pattern.
     * Returns true on success, false on I/O error.
     */
    static bool _writeFileAtomically(const std::string &target_path,
                                     const std::string &body);
};
