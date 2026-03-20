#include "../includes/PathResolver.hpp"

/**
 * example:
 *  loc.path = "/files"
	request_path = "/files/index.html"
	loc.root = "./www/files"
 */
std::string PathResolver::resolveFilePath(const Location &loc,
                                          const std::string &request_path) {
    if (request_path == loc.path)
        return loc.root + "/" + loc.index;

    return loc.root + request_path.substr(loc.path.length()); // extract from loc.path.len till end of str
}
