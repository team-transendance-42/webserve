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

// _fd is private: not access from outside: no getter: todo : what is better?
int EpollLoop::wait(struct epoll_event *events, int max_events, int timeout_ms) const {
	return epoll_wait(_fd, events, max_events, timeout_ms);
}

void EpollLoop::add(int fd, uint32_t events) const {
	struct epoll_event ev;
	ev.events = events;
	ev.data.fd = fd;
	if (epoll_ctl(_fd, EPOLL_CTL_ADD, fd, &ev) < 0)
		throw std::runtime_error("epoll_ctl ADD failed: "
								 + std::string(strerror(errno)));
}

void EpollLoop::mod(int fd, uint32_t events) const {
	struct epoll_event ev;
	ev.events = events;
	ev.data.fd = fd;
	if (epoll_ctl(_fd, EPOLL_CTL_MOD, fd, &ev) < 0)
		throw std::runtime_error("epoll_ctl MOD failed: "
								 + std::string(strerror(errno)));
}

void EpollLoop::del(int fd) const {
	if (epoll_ctl(_fd, EPOLL_CTL_DEL, fd, NULL) < 0)
		std::cerr << "[epoll] DEL failed fd=" << fd
				  << ": " << strerror(errno) << "\n";
}
