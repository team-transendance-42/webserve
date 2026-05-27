#include "../includes/ErrorResponseBuilder.hpp"
#include "../includes/StaticFileHandler.hpp"

HttpResponse ErrorResponseBuilder::buildErrorResponse(int code, const ServerConfig &config) {
	std::map<int, std::string>::const_iterator ep = config.errorPages.find(code);
	if (ep != config.errorPages.end()) {
		HttpResponse custom = StaticFileHandler::serveStatic(ep->second);
		if (custom.statusCode == 200) {
			custom.setStatus(code);
			return custom;
		}
	}
	return _defaultErrorResponse(code);
}

/* Builds the fallback error response when no custom error_page is configured.
   Every HTTP error code the server can emit needs a case here — without one
   the default falls through to 500, which is wrong (e.g. a real 415 would
   show "500 Internal Server Error" in the browser instead). */
HttpResponse ErrorResponseBuilder::_defaultErrorResponse(int code) {
	switch (code) {
		case 400: return HttpResponse::make_400();
		case 403: return HttpResponse::make_403();
		case 404: return HttpResponse::make_404();
		case 405: return HttpResponse::make_405();
		case 408: return HttpResponse::make_408();
		case 409: return HttpResponse::make_409();
		case 413: return HttpResponse::make_413();
		case 415: return HttpResponse::make_415(); // unsupported Content-Type
		case 501: return HttpResponse::make_501(); // valid method, not supported by this server
		case 504: return HttpResponse::make_504(); // CGI script timed out
		default:  return HttpResponse::make_500();
	}
}
