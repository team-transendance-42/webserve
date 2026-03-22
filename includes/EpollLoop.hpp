#pragma once

#include <cstdint>
#include <sys/epoll.h>

/**
 * EpollLoop is a simple wrapper around the Linux epoll API to manage an epoll instance.
 * It provides methods to initialize the epoll instance, wait for events, and add/modify/delete file descriptors.
 * This class abstracts away the low-level details of using epoll and provides a cleaner interface for the server's main loop.
 */
class EpollLoop {
public:
	EpollLoop();
	~EpollLoop();

	EpollLoop(const EpollLoop &) = delete;
	EpollLoop &operator=(const EpollLoop &) = delete;

	void init();
	int wait(struct epoll_event *events, int max_events, int timeout_ms) const;
	void add(int fd, uint32_t events) const;
	void mod(int fd, uint32_t events) const;
	void del(int fd) const;

private:
	int _fd;
};
