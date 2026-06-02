#pragma once

#include <ctime>
#include <string>
#include <sys/types.h>
#include <unistd.h>

/*
 * Represents an active CGI execution session.
 * Owned by a Client; heap-allocated when a CGI request begins,
 * deleted when the CGI process completes and the response is written.
 * 
 * The session tracks:
 * - Child process ID
 * - Pipe file descriptors for stdin/stdout (connected to the child)
 * - Request body buffer and write offset (for chunked writes to stdin)
 * - Response output buffer (accumulates CGI stdout)
 * - Deadline for timeout (in time_t seconds)
 * - State machine (which events to listen for)
 */
struct CgiSession {
    enum State {
        STATE_BODY_WRITE,     /* Writing request body to stdin */
        STATE_READING_OUTPUT, /* Reading CGI stdout */
        STATE_FINALIZING      /* Waiting for final EOF/waitpid */
    };

    pid_t       pid;
    int         stdin_fd;         /* Write end of pipe to CGI stdin */
    int         stdout_fd;        /* Read end of pipe from CGI stdout */
    std::string body;             /* Request body to write to stdin */
    std::size_t body_written;     /* Bytes written so far */
    std::string output;           /* Accumulated CGI stdout */
    std::size_t max_output_bytes; /* Max size before truncation */
    time_t      deadline;         /* Absolute time when CGI should timeout */
    State       state;            /* Current state (for determining which events matter) */
    int         exit_code;        /* Set when child exits (for final response code) */

    CgiSession()
        : pid(-1),
          stdin_fd(-1),
          stdout_fd(-1),
          body(),
          body_written(0),
          output(),
          max_output_bytes(1024 * 1024),
          deadline(0),
          state(STATE_BODY_WRITE),
          exit_code(-1) {}

    ~CgiSession() {
        if (stdin_fd >= 0) close(stdin_fd);
        if (stdout_fd >= 0) close(stdout_fd);
    }

    bool isBodyComplete() const {
        return body_written >= body.size();
    }
};
