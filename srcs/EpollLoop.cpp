#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/epoll.h>
#include <unistd.h>

#include "../includes/EpollLoop.hpp"

EpollLoop::EpollLoop() : _fd(-1) {}

EpollLoop::~EpollLoop() {
	if (_fd >= 0) close(_fd);
}

void EpollLoop::init() {
	_fd = epoll_create1(0);
	if (_fd < 0)
		throw std::runtime_error("epoll_create1() failed: "
								 + std::string(strerror(errno)));
}

/**
 * Waits for events on the epoll file descriptor.
 * @return Number of events returned.
 * wrapper which abstracts away _fd
 */
int EpollLoop::wait(struct epoll_event *events, int maxEvents, int timeoutMs) const {
	return epoll_wait(_fd, events, maxEvents, timeoutMs);
}

/*
epoll_ctl(_fd, EPOLL_CTL_ADD, fd, &ev) adds the file descriptor fd to the epoll instance fd and tells epoll to watch it for the events specified in ev (like read or write readiness).
*/
void EpollLoop::add(int fd, uint32_t events) const {
	if (_fd < 0)
        throw std::runtime_error("EpollLoop::add called before init()");
	struct epoll_event ev = makeEvent(fd, events);
	if (epoll_ctl(_fd, EPOLL_CTL_ADD, fd, &ev) < 0)
		throw std::runtime_error("epoll_ctl ADD failed: "
									 + std::string(strerror(errno)));
}

/**
 * Modifies the events for a file descriptor in the epoll set.
 * @param fd File descriptor to modify.
 * @param events Events to set.(read, write, etc.)
 */
void EpollLoop::mod(int fd, uint32_t events) const {
	if (_fd < 0)
        throw std::runtime_error("EpollLoop::mod called before init()");
	struct epoll_event ev = makeEvent(fd, events);
	if (epoll_ctl(_fd, EPOLL_CTL_MOD, fd, &ev) < 0)
		throw std::runtime_error("epoll_ctl MOD failed: "
									 + std::string(strerror(errno)));
}

/**
 * Removes a file descriptor from the epoll set.
 * @param fd File descriptor to remove.
 EBADF(fd was already closed) — not an error.
 */
void EpollLoop::del(int fd) const {
	if (_fd < 0) return;
	if (epoll_ctl(_fd, EPOLL_CTL_DEL, fd, NULL) < 0 && errno != EBADF)
		std::cerr << "[epoll] DEL failed fd=" << fd
				  << ": " << strerror(errno) << "\n";
}

/*
no need to delete or free anything after using memset on a stack variable like struct epoll_event
struct epoll_event: built-in struct. holds info  about what events to watch for on fd (like read, write, etc.) and some user data (here, the fd).
*/
struct epoll_event EpollLoop::makeEvent(int fd, uint32_t events) {
	struct epoll_event ev;
	std::memset(&ev, 0, sizeof(ev));
	ev.events = events;
	ev.data.fd = fd;
	return ev;
}
