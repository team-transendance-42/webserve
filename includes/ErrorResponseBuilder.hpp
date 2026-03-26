#pragma once

#include "ServerConfig.hpp"
#include "HttpResponse.hpp"

/**
 * generating HTTP error responses.
 * Checks for custom error pages in the server config; if not found, builds a default error response body.
 * Used by request handlers to return 4xx/5xx responses in a consistent way.
 */
class ErrorResponseBuilder {
public:
	static HttpResponse buildErrorResponse(int code, const ServerConfig &config);

private:
	static HttpResponse _defaultErrorResponse(int code);
};
