#include "../includes/CgiExecutor.hpp"

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <poll.h>
#include <sstream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

long long nowMs() {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return (0);
    }
    return (static_cast<long long>(ts.tv_sec) * 1000LL + static_cast<long long>(ts.tv_nsec / 1000000LL));
}

std::string toString(std::size_t value) {
    std::ostringstream oss;
    oss << value;
    return (oss.str());
}

CgiExecutor::CgiExecutor(std::size_t max_output_bytes, int timeout_ms)
    : _max_output_bytes(max_output_bytes), _timeout_ms(timeout_ms) {
}

std::string CgiExecutor::sanitizeHeaderName(const std::string& key) {
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

bool CgiExecutor::writeAll(int fd, const std::string& data) {
    std::size_t offset = 0;
    while (offset < data.size()) {
        ssize_t written = write(fd, data.data() + offset, data.size() - offset);
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return (false);
        }
        if (written == 0) {
            return (false);
        }
        offset += static_cast<std::size_t>(written);
    }
    return (true);
}

CgiResult CgiExecutor::execute(const CgiRequest& request, const Location& location) const {
    CgiResult result;

    if (!location.hasCgi()) {
        result.error_message = "location is missing CGI configuration";
        return (result);
    }

    int stdinPipe[2] = {-1, -1};
    int stdoutPipe[2] = {-1, -1};

    if (pipe(stdinPipe) != 0 || pipe(stdoutPipe) != 0) {
        result.error_message = "failed to create CGI pipes";
        if (stdinPipe[0] >= 0)
            close(stdinPipe[0]);
        if (stdinPipe[1] >= 0)
            close(stdinPipe[1]);
        if (stdoutPipe[0] >= 0)
            close(stdoutPipe[0]);
        if (stdoutPipe[1] >= 0)
            close(stdoutPipe[1]);
        return (result);
    }

    pid_t pid = fork();
    if (pid < 0) {
        result.error_message = "failed to fork CGI process";
        close(stdinPipe[0]);
        close(stdinPipe[1]);
        close(stdoutPipe[0]);
        close(stdoutPipe[1]);
        return (result);
    }

    if (pid == 0) {
        if (dup2(stdinPipe[0], STDIN_FILENO) < 0 || dup2(stdoutPipe[1], STDOUT_FILENO) < 0) {
            _exit(127);
        }

        close(stdinPipe[0]);
        close(stdinPipe[1]);
        close(stdoutPipe[0]);
        close(stdoutPipe[1]);

        std::vector<std::string> envStorage;
        envStorage.reserve(16 + request.headers.size());

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

        for (std::map<std::string, std::string>::const_iterator it = request.headers.begin(); it != request.headers.end(); ++it) {
            std::string key = sanitizeHeaderName(it->first);
            if (key == "CONTENT_TYPE" || key == "CONTENT_LENGTH") {
                continue;
            }
            envStorage.push_back("HTTP_" + key + "=" + it->second);
        }

        std::vector<char*> envp;
        envp.reserve(envStorage.size() + 1);
        for (std::size_t i = 0; i < envStorage.size(); ++i) {
            envp.push_back(const_cast<char*>(envStorage[i].c_str()));
        }
        envp.push_back(NULL);

        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(location.cgi_pass.c_str()));
        argv.push_back(const_cast<char*>(request.script_path.c_str()));
        argv.push_back(NULL);

        execve(location.cgi_pass.c_str(), &argv[0], &envp[0]);
        _exit(127);
    }

    close(stdinPipe[0]);
    close(stdoutPipe[1]);

    bool writeOk = writeAll(stdinPipe[1], request.body);
    close(stdinPipe[1]);
    if (!writeOk) {
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
        close(stdoutPipe[0]);
        result.error_message = "failed to write request body to CGI stdin";
        return (result);
    }

    pollfd pfd;
    pfd.fd = stdoutPipe[0];
    pfd.events = POLLIN;
    pfd.revents = 0;

    const long long startedAt = nowMs();
    std::string output;
    char buffer[4096];

    while (true) {
        long long elapsed = nowMs() - startedAt;
        if (_timeout_ms >= 0 && elapsed >= _timeout_ms) {
            result.timed_out = true;
            kill(pid, SIGKILL);
            break;
        }

        int waitMs = 100;
        if (_timeout_ms >= 0) {
            long long remaining = _timeout_ms - elapsed;
            if (remaining < waitMs) {
                waitMs = static_cast<int>(remaining > 0 ? remaining : 0);
            }
        }

        int pollRc = poll(&pfd, 1, waitMs);
        if (pollRc > 0 && (pfd.revents & POLLIN)) {
            ssize_t n = read(stdoutPipe[0], buffer, sizeof(buffer));
            if (n > 0) {
                if (output.size() + static_cast<std::size_t>(n) > _max_output_bytes) {
                    std::size_t allowed = _max_output_bytes - output.size();
                    output.append(buffer, allowed);
                    result.output_truncated = true;
                    kill(pid, SIGKILL);
                    break;
                }
                output.append(buffer, static_cast<std::size_t>(n));
            } else if (n == 0) {
                break;
            } else if (errno != EINTR) {
                result.error_message = "failed while reading CGI stdout";
                kill(pid, SIGKILL);
                break;
            }
        } else if (pollRc < 0 && errno != EINTR) {
            result.error_message = "poll failed while waiting for CGI stdout";
            kill(pid, SIGKILL);
            break;
        }

        int status = 0;
        pid_t done = waitpid(pid, &status, WNOHANG);
        if (done == pid) {
            if (WIFEXITED(status)) {
                result.exit_code = WEXITSTATUS(status);
            } else {
                result.exit_code = 128;
            }

            // Drain any buffered data left in the pipe after process exit.
            while (true) {
                ssize_t n = read(stdoutPipe[0], buffer, sizeof(buffer));
                if (n > 0) {
                    if (output.size() + static_cast<std::size_t>(n) > _max_output_bytes) {
                        std::size_t allowed = _max_output_bytes - output.size();
                        output.append(buffer, allowed);
                        result.output_truncated = true;
                        break;
                    }
                    output.append(buffer, static_cast<std::size_t>(n));
                } else {
                    break;
                }
            }
            break;
        }
    }

    close(stdoutPipe[0]);

    int status = 0;
    waitpid(pid, &status, 0);
    if (result.exit_code < 0 && WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
    }

    result.raw_output = output;
    if (result.timed_out) {
        result.error_message = "CGI execution timed out";
        return (result);
    }

    if (result.error_message.empty() && result.exit_code == 0) {
        result.success = true;
    } else if (result.error_message.empty()) {
        result.error_message = "CGI process exited with non-zero status";
    }

    return (result);
}
