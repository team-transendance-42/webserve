#pragma once

#include "config/Config.hpp"
#include "CgiSession.hpp"

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

/*
 * Refactored CgiExecutor: spawns CGI processes and tracks them via the EventLoop.
 * No blocking poll() or waitpid() anymore — all I/O driven by the shared epoll.
 */
class CgiExecutor {
public:
    CgiExecutor(std::size_t max_output_bytes = 1024 * 1024, int timeout_ms = 5000);

    /* Start a CGI session: fork, setup pipes, execve.
       Returns a heap-allocated CgiSession on success; nullptr on failure.
       The session is ready for epoll registration immediately after. */
    CgiSession *start(const CgiRequest &request, const Location &location) const;

    /* Parse CGI output into HTTP headers and body.
       static so it can be called from EventLoop::_finalizeCgi.
       Returns true if output was successfully split into headers+body. */
    static bool parseOutput(const std::string &raw_output,
                           std::string &headers,
                           std::string &body);

private:
    std::size_t _max_output_bytes;
    int         _timeout_ms;

    static std::string sanitizeHeaderName(const std::string &key);
};

