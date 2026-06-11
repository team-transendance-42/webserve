#include "../includes/ErrorResponseBuilder.hpp"
#include "../includes/StaticFileHandler.hpp"

HttpResponse ErrorResponseBuilder::buildErrorResponse(int code, const ServerConfig &config) {
	std::map<int, std::string>::const_iterator ep = config.errorPages.find(code);
	if (ep != config.errorPages.end()) {
		std::string path = ep->second;
		if (!path.empty() && path[0] == '/')
			path.insert(0, ".");
		HttpResponse custom = StaticFileHandler::serveStatic(path);
		if (custom.statusCode == 200) {
			custom.setStatus(code);
			return custom;
		}
	}
	return _defaultErrorResponse(code);
}

HttpResponse ErrorResponseBuilder::_defaultErrorResponse(int code) {
	switch (code) {
		case 400: return HttpResponse::make_err_page("Bad Request", 400);
		case 403: return HttpResponse::make_err_page("Forbidden", 403);
		case 404: return HttpResponse::make_err_page("Not Found", 404);
		case 405: return HttpResponse::make_err_page("Method Not Allowed", 405);
		case 408: return HttpResponse::make_err_page("Request Timeout", 408);
		case 409: return HttpResponse::make_err_page("Conflict", 409);
		case 413: return HttpResponse::make_err_page("Payload Too Large", 413);
		case 415: return HttpResponse::make_err_page("Unsupported Media Type", 415);
		case 501: return HttpResponse::make_err_page("Not Implemented", 501);
		case 504: return HttpResponse::make_err_page("Gateway Timeout", 504);
		default:  return HttpResponse::make_err_page("Internal Server Error", 500);
	}
}
