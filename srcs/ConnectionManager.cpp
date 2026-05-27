#include <cerrno>
#include <cstring>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>

#include "../includes/HttpResponse.hpp"
#include "../includes/ConnectionManager.hpp"
#include "../includes/Listener.hpp"
#include "../includes/ProcessRequest.hpp"

/*
 * Constructor: stores references to the shared clients map and epoll callbacks.
 * epollMod/epollDel are lambdas passed in from Server so ConnectionManager can
 * change epoll watch mode or remove an fd without holding a pointer to Server.
 *
 * EPOLLOUT: socket is writable — send() will not block, safe to write response bytes.
 * EPOLLRDHUP: peer closed its write side (remote half-close) — client disconnected
 * or will send no more data. Combined EPOLLOUT | EPOLLRDHUP means: notify when
 * writable to send the response, but also catch disconnect while waiting.
 */
ConnectionManager::ConnectionManager(std::map<int, Client *> &clients,
					 std::map<int, Listener *> &clientToListener,
					 std::function<void(int, uint32_t)> epollMod,
					 std::function<void(int)> epollDel)
	: _clients(clients),
	  _clientToListener(clientToListener),
	  _epollMod(std::move(epollMod)), // no longer need the original value and want efficiency
	  _epollDel(std::move(epollDel)) {}

/**
 *  Drains the socket in a non-blocking loop, feeding each chunk into the incremental HTTP parser.
 *  PARSE_INCOMPLETE is handled implicitly: the loop continues calling recv until EAGAIN/EWOULDBLOCK,
 *  then returns — epoll re-fires on the next incoming data and resumes feeding the same client.request.
 *  PARSE_ERROR → 400 and close. COMPLETE → hand off to the request processor and switch to write mode.
 */
void ConnectionManager::readClient(Client &client, std::size_t) {
	char chunk[4096]; // stack array: no heap allocation, no cleanup, already in CPU cache
	client.lastTimestamp = std::time(nullptr);
	while (true) {
		ssize_t bytes = recv(client.fd, chunk, sizeof(chunk), 0);

		if (bytes < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) 	break; // try again
			closeClient(client.fd);
			return;
		}
		if (bytes == 0) {
			closeClient(client.fd);
			return;
		}

		ParseResult result = client.request.feed(chunk, static_cast<size_t>(bytes));

		if (result == PARSE_ERROR) {
			client.writeBuf  = HttpResponse::make_400().serialize();
			client.keep_alive = false;
			_epollMod(client.fd, EPOLLOUT | EPOLLRDHUP); // switch to write mode to send the 400 response, but also detect disconnect while waiting
			return;
		}

		if (result == COMPLETE) {
			_clientToListener.at(client.fd)->processRequest().handle(client);
			_epollMod(client.fd, EPOLLOUT | EPOLLRDHUP); // | is bitwise OR, so both flags are enabled at once; epoll reports whenever any enabled flag occurs.
			return;
		}
	}
}

/**
 *  Flushes the write buffer to the socket in a non-blocking loop.
 *  On EAGAIN/EWOULDBLOCK, returns and lets epoll re-fire when the socket is writable again.
 *  On completion: if keep-alive, clears the request, re-arms for EPOLLIN, and checks whether
 *  a pipelined request is already buffered (tryParse). Otherwise closes the connection.
 */
void ConnectionManager::writeClient(Client &client) {
	client.lastTimestamp = std::time(nullptr); // update last activity time on each write
	while (!client.writeBuf.empty()) {
		ssize_t sent = send(client.fd,
							client.writeBuf.c_str(),
							client.writeBuf.size(), 0);
		if (sent < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) return;
			closeClient(client.fd);
			return;
		}
		client.writeBuf.erase(0, static_cast<size_t>(sent));
	}

	if (client.keep_alive) { // connection stays open for next req
		client.request.clear();
		_epollMod(client.fd, EPOLLIN | EPOLLRDHUP);

		ParseResult res = client.request.tryParse();
		if (res == COMPLETE) {
			_clientToListener.at(client.fd)->processRequest().handle(client);
			_epollMod(client.fd, EPOLLOUT | EPOLLRDHUP);
		} else if (res == PARSE_ERROR) {
			client.writeBuf = HttpResponse::make_400().serialize();
			client.keep_alive = false;
			_epollMod(client.fd, EPOLLOUT | EPOLLRDHUP);
		}
	} else {
		closeClient(client.fd);
	}
}

/*
 * closeClient: removes a client cleanly — unregisters fd from epoll, closes the
 * socket, frees the heap Client object, and erases it from the clients map.
 * Safe to call if fd is already gone (early return if not found).
 * _epollDel is a lambda from Server: it calls _epoll.del(fd) without ConnectionManager
 * needing a direct pointer to EpollLoop.
 */
void ConnectionManager::closeClient(int fd) {
	std::map<int, Client *>::iterator it = _clients.find(fd);
	if (it == _clients.end()) return; // already closed
	_epollDel(fd);
	close(fd);
	delete it->second;
	_clients.erase(it);
	_clientToListener.erase(fd);
}
