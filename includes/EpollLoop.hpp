#pragma once

#include <cstdint>
#include <sys/epoll.h>

/**
 * a simple wrapper around the Linux epoll API to manage an epoll instance.
 * provides methods to initialize the epoll instance, wait for events, and add/modify/delete file descriptors.
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
