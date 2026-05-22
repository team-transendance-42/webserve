#include <cerrno>
#include <cctype>
#include <climits>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sys/types.h>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

#include "../includes/CgiExecutor.hpp"
#include "../includes/ErrorResponseBuilder.hpp"
#include "../includes/ProcessRequest.hpp"
#include "../includes/StaticFileHandler.hpp"
#include "../includes/UploadHandler.hpp"
#include "../includes/config/Config.hpp"

// Check if file extension matches cgi_extension for this location
bool ProcessRequest::_shouldExecuteCgi(const Location &loc, const std::string &filepath) const {
    if (loc.cgi_extension.empty()) {
        return false;
    }
    
    // Check if filepath ends with cgi_extension (e.g., ".py")
    if (filepath.size() >= loc.cgi_extension.size()) {
        std::string ext = filepath.substr(filepath.size() - loc.cgi_extension.size());
        return ext == loc.cgi_extension;
    }
    
    return false;
}

// Execute CGI script and write response to client
bool ProcessRequest::_executeCgiOrError(const HttpRequest &req,
                                        const Location &loc,
                                        const std::string &filepath,
                                        Client &client) const {
    CgiRequest cgiReq = _buildCgiRequest(req, filepath);
    CgiExecutor executor;
    CgiResult result = executor.execute(cgiReq, loc);
    
    if (!result.success) {
        // If CGI execution failed, return 500
        HttpResponse response;
        response.setStatus(500)
                .setBody("<html><body><h1>500 Internal Server Error</h1><p>CGI execution failed</p></body></html>", "text/html");
        client.writeBuf = response.serialize();
        return true;
    }
    
    if (result.timed_out) {
        // CGI script timed out, return 504
        HttpResponse response;
        response.setStatus(504)
                .setBody("<html><body><h1>504 Gateway Timeout</h1><p>CGI script took too long</p></body></html>", "text/html");
        client.writeBuf = response.serialize();
        return true;
    }
    
    // Parse CGI output into HTTP response
    HttpResponse response;
    if (!_buildHttpResponseFromCgiOutput(result.raw_output, response)) {
        // If parsing failed, treat output as plain text body
        response.setStatus(200)
                .setBody(result.raw_output, "text/plain");
    }
    
    client.writeBuf = response.serialize();
    return true;
}

// Build CgiRequest from HttpRequest
CgiRequest ProcessRequest::_buildCgiRequest(const HttpRequest &req,
                                            const std::string &filepath) const {
    CgiRequest cgiReq;
    cgiReq.method = methodToString(req.method);
    cgiReq.script_path = filepath;
    cgiReq.script_name = req.path;
    cgiReq.query_string = req.query_string;
    cgiReq.body = req.body;
    
    // Get Content-Type header and Content-Length from body
    std::string contentType = req.getHeader("content-type");
    cgiReq.content_type = (contentType.empty() ? "" : contentType);
    
    cgiReq.server_protocol = req.version;
    
    // Parse host header to get server_name and server_port
    std::string host = req.getHeader("host");
    size_t colon = host.find(':');
    if (colon != std::string::npos) {
        cgiReq.server_name = host.substr(0, colon);
        cgiReq.server_port = host.substr(colon + 1);
    } else {
        cgiReq.server_name = host;
        cgiReq.server_port = "80"; // default HTTP port
    }
    
    cgiReq.remote_addr = "127.0.0.1"; // localhost for now todo
    cgiReq.path_info = ""; // PATH_INFO for extra path after script
    cgiReq.headers = req.headers;
    
    return cgiReq;
}

// Parse CGI output (headers + blank line + body) into HttpResponse
bool ProcessRequest::_buildHttpResponseFromCgiOutput(const std::string &raw,
                                                     HttpResponse &response) const {
    // CGI output format: "Header: value\r\nHeader: value\r\n\r\nbody content"
    size_t blankLinePos = raw.find("\r\n\r\n");
    if (blankLinePos == std::string::npos) {
        // Try Unix line endings
        blankLinePos = raw.find("\n\n");
        if (blankLinePos == std::string::npos) {
            return false; // No valid CGI header/body separator found
        }
        // Parse headers up to \n\n
        std::string headerSection = raw.substr(0, blankLinePos);
        std::string body = raw.substr(blankLinePos + 2);
        
        // Set body
        response.setBody(body, "text/html");
        
        // Parse headers
        std::istringstream iss(headerSection);
        std::string line;
        bool statusSet = false;
        while (std::getline(iss, line)) {
            if (!line.empty() && line[line.size() - 1] == '\r') {
                line.erase(line.size() - 1);
            }
            size_t colonPos = line.find(':');
            if (colonPos != std::string::npos) {
                std::string key = line.substr(0, colonPos);
                std::string value = line.substr(colonPos + 2);
                
                if (key == "Status") {
                    int statusCode = atoi(value.c_str());
                    response.setStatus(statusCode);
                    statusSet = true;
                } else if (key != "Content-Length") {
                    response.setHeader(key, value);
                }
            }
        }
        
        if (!statusSet) {
            response.setStatus(200);
        }
        
        return true;
    }
    
    // Standard \r\n separator
    std::string headerSection = raw.substr(0, blankLinePos);
    std::string body = raw.substr(blankLinePos + 4);
    
    // Set body
    response.setBody(body, "text/html");
    
    // Parse headers
    size_t pos = 0;
    bool statusSet = false;
    while (pos < headerSection.size()) {
        size_t lineEnd = headerSection.find("\r\n", pos);
        if (lineEnd == std::string::npos) lineEnd = headerSection.size();
        
        std::string line = headerSection.substr(pos, lineEnd - pos);
        
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string key = line.substr(0, colonPos);
            std::string value = line.substr(colonPos + 2);
            
            if (key == "Status") {
                int statusCode = atoi(value.c_str());
                response.setStatus(statusCode);
                statusSet = true;
            } else if (key != "Content-Length") {
                response.setHeader(key, value);
            }
        }
        
        pos = lineEnd + 2;
    }
    
    if (!statusSet) {
        response.setStatus(200);
    }
    
    return true;
}
