#include <cerrno>
#include <cstring>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>

#include "../includes/HttpResponse.hpp"
#include "../includes/ConnectionManager.hpp"
#include "../includes/ProcessRequest.hpp"

/**
 * 	std::string a = "hello";
	std::string b = std::move(a); // b takes resources from a
	a is valid, but its content is unspecified (often "")
	An rvalue is a temporary value, usually with no stable name, typically used on the right side of assignment.

	EPOLLOUT: socket is writable now, so send should not block (you can write response bytes).
	EPOLLRDHUP: peer has closed its write side (remote half-close). For HTTP server logic, this usually means client disconnected or will send no more request body bytes. switching to EPOLLOUT | EPOLLRDHUP means: wait until response can be sent, but also detect client disconnect while waiting.
 */
ConnectionManager::ConnectionManager(std::map<int, Client *> &clients,
					 std::function<void(int, uint32_t)> epollMod,
					 std::function<void(int)> epollDel,
					 ProcessRequest &requestProcessor)
	: _clients(clients),
	  _epollMod(std::move(epollMod)), // no longer need the original value and want efficiency
	  _epollDel(std::move(epollDel)),
	  _processorRequest(requestProcessor) {}

/**
 *  Drains the socket in a non-blocking loop, feeding each chunk into the incremental HTTP parser.
 *  PARSE_INCOMPLETE is handled implicitly: the loop continues calling recv until EAGAIN/EWOULDBLOCK,
 *  then returns — epoll re-fires on the next incoming data and resumes feeding the same client.request.
 *  PARSE_ERROR → 400 and close. COMPLETE → hand off to the request processor and switch to write mode.
 */
void ConnectionManager::readClient(Client &client, std::size_t readBufSize) {
	std::string chunk(readBufSize, '\0'); // chunk is a var; create a string of length readBufSize, fill it with '\0' chars.
	client.lastTimestamp = std::time(0); // update last activity time on each read
	while (true) {
		ssize_t bytes = recv(client.fd, &chunk[0], chunk.size(), 0);

		if (bytes < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) 	break; // try again
			closeClient(client.fd);
			return;
		}
		if (bytes == 0) {
			closeClient(client.fd);
			return;
		}

		ParseResult result = client.request.feed(&chunk[0], static_cast<size_t>(bytes));

		if (result == PARSE_ERROR) {
			client.writeBuf  = HttpResponse::make_400().serialize();
			client.keep_alive = false;
			_epollMod(client.fd, EPOLLOUT | EPOLLRDHUP);
			return;
		}

		if (result == COMPLETE) {
			_processorRequest.handle(client);
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
	client.lastTimestamp = std::time(0); // update last activity time on each write
	while (!client.writeBuf.empty()) {
		ssize_t sent = send(client.fd,
							client.writeBuf.c_str(),
							client.writeBuf.size(), 0);
		if (sent <= 0) {
			if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return;
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
			_processorRequest.handle(client);
			_epollMod(client.fd, EPOLLOUT | EPOLLRDHUP);
		} else if (res == PARSE_ERROR) {
			client.writeBuf = HttpResponse::make_400().serialize(); // build obj->set fields->flatten to bytes->send bytes
			client.keep_alive = false;
			_epollMod(client.fd, EPOLLOUT | EPOLLRDHUP);
		}
	} else {
		closeClient(client.fd);
	}
}

/**
 *  Server passes a lambda that calls its own method.
	Real function body is Server::_epollDel(int fd).
	_epollDel in ConnectionManager is a callback handle, not a function definition.
 */

void ConnectionManager::closeClient(int fd) {
	std::map<int, Client *>::iterator it = _clients.find(fd);
	if (it == _clients.end()) return; // already closed
	_epollDel(fd);
	close(fd);
	delete it->second;
	_clients.erase(it);
}
