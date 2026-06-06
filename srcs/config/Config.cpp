#include "../../includes/config/Config.hpp"

/*
 * Match a URI to the most specific location block.
 * Returns a pointer to the matching location.
 *
 * nullptr if no match
 */
const Location *ServerConfig::matchLocation(const std::string &uri) const {
    const Location *best     = nullptr;
    size_t          best_len = 0;

    for (const auto &loc : locations) {
        if (uri.compare(0, loc.path.size(), loc.path) == 0) {
            if (loc.path.size() > best_len) {
                best_len = loc.path.size();
                best     = &loc;
            }
        }
    }
    return (best);
}
