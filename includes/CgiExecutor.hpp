#pragma once

#include "ServerConfig.hpp"

#include <cstddef>
#include <map>
#include <string>

struct CgiRequest {
    std::string method;
    std::string script_path;
    std::string script_name;
    std::string query_string;
    std::string path_info;
    std::string body;
    std::string content_type;
    std::string server_protocol;
    std::string server_name;
    std::string server_port;
    std::string remote_addr;
    std::map<std::string, std::string> headers;
};

struct CgiResult {
    bool        success;
    bool        timed_out;
    bool        output_truncated;
    int         exit_code;
    std::string raw_output;
    std::string error_message;

    CgiResult()
        : success(false),
          timed_out(false),
          output_truncated(false),
          exit_code(-1),
          raw_output(),
          error_message() {
    }
};

class CgiExecutor {
public:
    CgiExecutor(std::size_t max_output_bytes = 1024 * 1024, int timeout_ms = 5000);

    CgiResult execute(const CgiRequest& request, const Location& location) const;

private:
    std::size_t _max_output_bytes;
    int         _timeout_ms;

    static std::string sanitizeHeaderName(const std::string& key);
    static bool        writeAll(int fd, const std::string& data);
};
