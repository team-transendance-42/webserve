#include "../includes/CgiExecutor.hpp"

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sstream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

CgiExecutor::CgiExecutor(std::size_t max_output_bytes, int timeout_ms)
    : _max_output_bytes(max_output_bytes), _timeout_ms(timeout_ms) {
}

/* Sanitize header names by converting to uppercase and replacing '-' with '_' */
std::string CgiExecutor::sanitizeHeaderName(const std::string &key) {
    std::string result;
    result.reserve(key.size());

    for (std::size_t i = 0; i < key.size(); ++i) {
        unsigned char ch = static_cast<unsigned char>(key[i]);
        if (ch == '-') {
            result.push_back('_');
        } else {
            result.push_back(static_cast<char>(std::toupper(ch)));
        }
    }
    return (result);
}

/* Convert size_t to string */
static std::string toString(std::size_t value) {
    std::ostringstream oss;
    oss << value;
    return (oss.str());
}

/* Start a CGI session: fork and setup pipes/environment.
   Returns a heap-allocated CgiSession on success; nullptr on failure.
   All I/O from this point on is non-blocking and driven by EventLoop::tick. */
CgiSession *CgiExecutor::start(const CgiRequest &request, const Location &location) const {
    if (!location.hasCgi()) {
        return nullptr;
    }

    int stdinPipe[2] = {-1, -1};
    int stdoutPipe[2] = {-1, -1};

    /* Setup pipes for CGI stdin and stdout */
    if (pipe(stdinPipe) != 0 || pipe(stdoutPipe) != 0) {
        if (stdinPipe[0] >= 0) close(stdinPipe[0]);
        if (stdinPipe[1] >= 0) close(stdinPipe[1]);
        if (stdoutPipe[0] >= 0) close(stdoutPipe[0]);
        if (stdoutPipe[1] >= 0) close(stdoutPipe[1]);
        return nullptr;
    }

    /* Fork the process to execute the CGI script in the child */
    pid_t pid = fork();
    if (pid < 0) {
        close(stdinPipe[0]);
        close(stdinPipe[1]);
        close(stdoutPipe[0]);
        close(stdoutPipe[1]);
        return nullptr;
    }

    if (pid == 0) {
        /* Child process: setup environment and execute CGI script */
        if (dup2(stdinPipe[0], STDIN_FILENO) < 0 || dup2(stdoutPipe[1], STDOUT_FILENO) < 0) {
            _exit(127);
        }

        /* Close unused pipe ends in the child */
        close(stdinPipe[0]);
        close(stdinPipe[1]);
        close(stdoutPipe[0]);
        close(stdoutPipe[1]);

        std::vector<std::string> envStorage;
        envStorage.reserve(16 + request.headers.size());

        /* Standard CGI environment variables */
        envStorage.push_back("REQUEST_METHOD=" + request.method);
        envStorage.push_back("QUERY_STRING=" + request.query_string);
        envStorage.push_back("CONTENT_TYPE=" + request.content_type);
        envStorage.push_back("CONTENT_LENGTH=" + toString(request.body.size()));
        envStorage.push_back("SCRIPT_NAME=" + request.script_name);
        envStorage.push_back("SCRIPT_FILENAME=" + request.script_path);
        envStorage.push_back("PATH_INFO=" + request.path_info);
        envStorage.push_back("SERVER_PROTOCOL=" + request.server_protocol);
        envStorage.push_back("SERVER_NAME=" + request.server_name);
        envStorage.push_back("SERVER_PORT=" + request.server_port);
        envStorage.push_back("REMOTE_ADDR=" + request.remote_addr);
        envStorage.push_back("PATH=/usr/local/bin:/usr/bin:/bin");

        /* Add HTTP headers as environment variables with "HTTP_" prefix */
        for (std::map<std::string, std::string>::const_iterator it = request.headers.begin();
             it != request.headers.end(); ++it) {
            std::string key = sanitizeHeaderName(it->first);
            if (key == "CONTENT_TYPE" || key == "CONTENT_LENGTH") {
                continue;
            }
            envStorage.push_back("HTTP_" + key + "=" + it->second);
        }

        /* Convert envStorage to the format required by execve */
        std::vector<char *> envp;
        envp.reserve(envStorage.size() + 1);
        for (std::size_t i = 0; i < envStorage.size(); ++i) {
            envp.push_back(const_cast<char *>(envStorage[i].c_str()));
        }
        envp.push_back(NULL);

        std::vector<char *> argv;
        argv.push_back(const_cast<char *>(location.cgi_pass.c_str()));
        argv.push_back(const_cast<char *>(request.script_path.c_str()));
        argv.push_back(NULL);

        /* Execute the CGI script */
        execve(location.cgi_pass.c_str(), &argv[0], &envp[0]);
        _exit(127);
    }

    /* Parent process: close unused pipe ends and create session */
    close(stdinPipe[0]);
    close(stdoutPipe[1]);

    CgiSession *session = new CgiSession();
    session->pid = pid;
    session->stdin_fd = stdinPipe[1];
    session->stdout_fd = stdoutPipe[0];
    session->body = request.body;
    session->body_written = 0;
    session->max_output_bytes = _max_output_bytes;
    session->deadline = time(nullptr) + (_timeout_ms >= 0 ? _timeout_ms / 1000 : 60);
    session->state = CgiSession::STATE_BODY_WRITE;
    session->exit_code = -1;

    return session;
}

/* Parse CGI raw output into headers section and body.
   Handles both \r\n\r\n and \n\n as separators.
   Returns true if split was successful. */
bool CgiExecutor::parseOutput(const std::string &raw_output,
                              std::string &headers,
                              std::string &body) {
    size_t pos = raw_output.find("\r\n\r\n");
    if (pos != std::string::npos) {
        headers = raw_output.substr(0, pos);
        body = raw_output.substr(pos + 4);
        return true;
    }
    pos = raw_output.find("\n\n");
    if (pos != std::string::npos) {
        headers = raw_output.substr(0, pos);
        body = raw_output.substr(pos + 2);
        return true;
    }
    return false;
}

