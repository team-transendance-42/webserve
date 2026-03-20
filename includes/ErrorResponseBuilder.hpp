#pragma once

#include "ServerConfig.hpp"
#include "HttpResponse.hpp"

class ErrorResponseBuilder {
public:
	static HttpResponse buildErrorResponse(int code, const ServerConfig &config);

private:
	static HttpResponse _defaultErrorResponse(int code);
};
