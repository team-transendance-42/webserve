#include "../includes/ServerConfig.hpp"

ServerConfig createDefaultServerConfig() {
    ServerConfig config;

    config.host = "127.0.0.1";
    config.port = 8080;
    config.server_names.push_back("one");
    config.client_max_body_size = 1048576;
    config.default_server = true;

    // error pages
    config.error_pages[400] = "./www/errors/400.html"; //
    config.error_pages[403] = "./www/errors/403.html";
    config.error_pages[404] = "./www/errors/404.html";
    config.error_pages[500] = "./www/errors/500.html";

    // location /
    Location loc_root;
    loc_root.path = "/";
    loc_root.root = "./www/one";
    loc_root.index = "index.html";
    loc_root.autoindex = true;
    // loc_root.client_max_body_size = 1000000;
    loc_root.allowed_methods = {"GET", "POST"};
    config.locations.push_back(loc_root);

    // location /zombie_kittens
    Location loc_zombie;
    loc_zombie.path = "/zombie_kittens";
    loc_zombie.root = "./www/one/pages";
    loc_zombie.index = "zombie_kittens.html";
    loc_zombie.autoindex = false;
    loc_zombie.allowed_methods = {"GET"};
    config.locations.push_back(loc_zombie);

    // location /game_start
    Location loc_game;
    loc_game.path = "/game_start";
    loc_game.root = "./www/one/pages";
    loc_game.index = "game_start.html";
    loc_game.autoindex = false;
    loc_game.allowed_methods = {"GET"};
    config.locations.push_back(loc_game);

    // location /play (redirect): 
    Location loc_play;
    loc_play.path = "/play";
    loc_play.redirect_code = 301;
    loc_play.redirect_url = "/game_start";
    loc_play.allowed_methods = {"GET"};
    config.locations.push_back(loc_play);

    /**
	 *  test not authorized access to /admin
	 *  because config says: deny_all = true
	*/	
    Location secret;
    secret.path = "/secret";
    secret.root = "./www/one/secret";
    secret.index = "index.html";
    secret.autoindex = false;
    secret.allowed_methods = {"GET"};
    secret.deny_all = true;
    config.locations.push_back(secret);

    // test client max body size with /upload

    // test chmod 000 with
    Location notAllowed;
    notAllowed.path = "/notAllowed";
    notAllowed.root = "./www/notAllowed";
    notAllowed.index = "index.html";
    notAllowed.allowed_methods = {"GET"};
    config.locations.push_back(notAllowed);

    // TODO: disable autoindex for /files to test 403 when no index file: todo: got not mine 403..
    Location files;
    files.path = "/files";
    files.root = "./www/files";
    // files.index = "index.html";
    files.autoindex = false;
    files.allowed_methods = {"GET"};
    config.locations.push_back(files);  

    // expect to see files structure in www/files
    Location files_auto;
    files_auto.path = "/files_auto";
    files_auto.root = "./www/files";
    files_auto.autoindex = true;
    files_auto.allowed_methods = {"GET"};
    config.locations.push_back(files_auto);

    // test JSON API
    Location api;
    api.path = "/api/data_json";
    api.root = "./www/api";
    api.index = "data.json";
    api.autoindex = false;
    api.allowed_methods = {"GET"};
    config.locations.push_back(api);

    return config;
}

 // find longest matching location for a URI: standard way for servers(nginx)
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
    return best;
}