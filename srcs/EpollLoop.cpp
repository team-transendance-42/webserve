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

/* Creates the epoll instance. Closes any existing fd first to prevent a leak
   if called twice — the old epoll fd would otherwise be abandoned. */
void EpollLoop::init() {
	if (_fd >= 0) { close(_fd); _fd = -1; }
	_fd = epoll_create1(0);
	if (_fd < 0)
		throw std::runtime_error("epoll_create1() failed: "
								 + std::string(strerror(errno)));
}

/* Blocks until events are ready or timeoutMs elapses. Returns event count or -1.
   Guard added for consistency with add()/mod() — calling before init() is a
   programming error and should throw, not silently pass -1 to epoll_wait. */
int EpollLoop::wait(struct epoll_event *events, int maxEvents, int timeoutMs) const {
	if (_fd < 0)
		throw std::runtime_error("EpollLoop::wait called before init()");
	return epoll_wait(_fd, events, maxEvents, timeoutMs);
}

/* Registers fd with epoll for the given events. Returns false + logs on epoll_ctl
   failure (e.g. ENOSPC — fd limit hit under siege) so the caller can close/skip the
   fd without crashing the server.
   Throw kept only for uninitialized epoll (programming error). */
bool EpollLoop::add(int fd, uint32_t events) const {
	if (_fd < 0)
		throw std::runtime_error("EpollLoop::add called before init()");
	struct epoll_event ev = makeEvent(fd, events);
	if (epoll_ctl(_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
		int err = errno;
		std::cerr << "[epoll] ADD failed fd=" << fd
				  << " (" << strerror(err) << ")\n";
		return false;
	}
	return true;
}

/* Modifies the watched events for fd in the epoll set. Returns false + logs on failure. */
bool EpollLoop::mod(int fd, uint32_t events) const {
	if (_fd < 0)
		throw std::runtime_error("EpollLoop::mod called before init()");
	struct epoll_event ev = makeEvent(fd, events);
	if (epoll_ctl(_fd, EPOLL_CTL_MOD, fd, &ev) < 0) {
		int err = errno;
		std::cerr << "[epoll] MOD failed fd=" << fd
				  << " (" << strerror(err) << ")\n";
		return false;
	}
	return true;
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
