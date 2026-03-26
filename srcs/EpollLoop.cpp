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

// todo: need to catch somewhere this throw
void EpollLoop::init() {
	_fd = epoll_create1(0);
	if (_fd < 0)
		throw std::runtime_error("epoll_create1() failed: "
								 + std::string(strerror(errno)));
}

/**
 * Waits for events on the epoll file descriptor.
 * @param events Pointer to the array of epoll_event structures.
 * @param maxEvents Maximum number of events to return.
 * @param timeoutMs Timeout in milliseconds.
 * @return Number of events returned.
 * wrapper which abstracts away _fd
 */
int EpollLoop::wait(struct epoll_event *events, int maxEvents, int timeoutMs) const {
	return epoll_wait(_fd, events, maxEvents, timeoutMs);
}

void EpollLoop::add(int fd, uint32_t events) const {
	struct epoll_event ev = make_event(fd, events);
	if (epoll_ctl(_fd, EPOLL_CTL_ADD, fd, &ev) < 0)
		throw std::runtime_error("epoll_ctl ADD failed: "
									 + std::string(strerror(errno)));
}

/**
 * Modifies the events for a file descriptor in the epoll set.
 * @param fd File descriptor to modify.
 * @param events Events to set.
 */
void EpollLoop::mod(int fd, uint32_t events) const {
	struct epoll_event ev = make_event(fd, events);
	if (epoll_ctl(_fd, EPOLL_CTL_MOD, fd, &ev) < 0)
		throw std::runtime_error("epoll_ctl MOD failed: "
									 + std::string(strerror(errno)));
}

/**
 * Removes a file descriptor from the epoll set.
 * @param fd File descriptor to remove.
 */
void EpollLoop::del(int fd) const {
	if (epoll_ctl(_fd, EPOLL_CTL_DEL, fd, NULL) < 0)
		std::cerr << "[epoll] DEL failed fd=" << fd
				  << ": " << strerror(errno) << "\n";
}

struct epoll_event EpollLoop::make_event(int fd, uint32_t events) {
	struct epoll_event ev;
	std::memset(&ev, 0, sizeof(ev));
	ev.events = events;
	ev.data.fd = fd;
	return ev;
}
