#pragma once

#include "config/Config.hpp"
#include "HttpResponse.hpp"

class ErrorResponseBuilder {
public:
	static HttpResponse buildErrorResponse(int code, const ServerConfig &config);

private:
	static HttpResponse _defaultErrorResponse(int code);
};
