#include "../includes/ErrorResponseBuilder.hpp"

#include "../includes/StaticFileHandler.hpp"

HttpResponse ErrorResponseBuilder::buildErrorResponse(int code,
													  const ServerConfig &config) {
	std::map<int, std::string>::const_iterator ep = config.error_pages.find(code);
	if (ep != config.error_pages.end()) {
		HttpResponse custom = StaticFileHandler::serveStatic(ep->second);
		if (custom.status_code == 200) {
			custom.set_status(code);
			return custom;
		}
	}
	return _defaultErrorResponse(code);
}

HttpResponse ErrorResponseBuilder::_defaultErrorResponse(int code) {
	switch (code) {
		case 400: return HttpResponse::make_400();
		case 403: return HttpResponse::make_403();
		case 404: return HttpResponse::make_404();
		case 405: return HttpResponse::make_405();
		case 413: return HttpResponse::make_413();
		case 500: return HttpResponse::make_500();
		default: return HttpResponse::make_500();
	}
}
