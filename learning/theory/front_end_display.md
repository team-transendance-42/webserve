The www directory is connected to the backend through the logic in the HttpResponse class, specifically in the _resolve() and _serveFile() methods.

When a request comes in (e.g., GET /about.html), our server parses the path (e.g., /about.html).

In HttpResponse::_resolve(), we build the absolute path to the requested file by joining the configured root (which is usually set to www or a similar directory) with the request path: std::string absPath = _config.root + reqPath;