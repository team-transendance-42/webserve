#include "../includes/ServerConfig.hpp"

ServerConfig createDefaultServerConfig() {
    ServerConfig config;

    config.host = "localhost";
    config.port = 8080;
    config.server_names.push_back("one");
    config.clientMaxBodySize= 1048576;
    config.default_server = true;

    // location /secrets (403: NO Permission)
    Location secrets;
    secrets.path = "/secrets";
    secrets.root = "./www/secrets";
    secrets.index = "index.html";
    secrets.autoindex = false;
    secrets.allowedMethod = {"GET"};
    secrets.denyAll = true; // NO Permission
    config.locations.push_back(secrets);

    // error pages
	// todo: add with 300 pages....
    config.errorPages[400] = "./www/errors/400.html"; //
    config.errorPages[403] = "./www/errors/403.html";
    config.errorPages[404] = "./www/errors/404.html";
    config.errorPages[405] = "./www/errors/405.html";
    config.errorPages[413] = "./www/errors/413.html";
    config.errorPages[500] = "./www/errors/500.html";

    // location / : expect to see www/one/index.html
    Location locRoot;
    locRoot.path = "/";
    locRoot.root = "./www/one";
    locRoot.index = "index.html";
    locRoot.autoindex = false;
    locRoot.allowedMethod = {"GET", "POST"};
    config.locations.push_back(locRoot);

    // location /zombie_kittens
    Location locZombie;
    locZombie.path = "/zombie_kittens";
    locZombie.root = "./www/one/pages";
    locZombie.index = "zombieKittens.html";
    locZombie.autoindex = false;
    locZombie.allowedMethod = {"GET"};
    config.locations.push_back(locZombie);

    // location /game_start
    Location locGame;
    locGame.path = "/game_start";
    locGame.root = "./www/one/pages";
    locGame.index = "gameStart.html";
    locGame.autoindex = false;
    locGame.allowedMethod = {"GET"};
    config.locations.push_back(locGame);

    // location /zkAplyForm (GET page + POST demo submit)
    Location locAply;
    locAply.path = "/zk_apply_form";
    locAply.root = "./www/one";
    locAply.index = "pages/zkApplyForm.html";
    locAply.autoindex = false;
    locAply.allowedMethod = {"GET", "POST"};
    config.locations.push_back(locAply);

    // location /play (redirect): 
    Location locPlay;
    locPlay.path = "/play";
    locPlay.redirect_code = 301;
    locPlay.redirect_url = "/game_start";
    locPlay.allowedMethod = {"GET"};
    config.locations.push_back(locPlay);

    /**
	 *  test not authorized access to /admin
	 *  because config says: denyAll = true 
     get 403: forbidden (e.g., /admin with denyAll=true, or chmod 000 on a file/dir)
	*/	
    Location secret;
    secret.path = "/secret";
    secret.root = "./www/one/secret";
    secret.index = "index.html";
    secret.autoindex = false;
    secret.allowedMethod = {"GET"};
    secret.denyAll = true;
    config.locations.push_back(secret);

    // test client max body size

    // test chmod 000 with
    Location notAllowed;
    notAllowed.path = "/not_allowed";
    notAllowed.root = "./www/notAllowed";
    notAllowed.index = "index.html";
    notAllowed.allowedMethod = {"GET"};
    config.locations.push_back(notAllowed);

    // Location postBodyTooLarge: serves a page that submits oversized POST body
    Location postBodyTooLarge;
    postBodyTooLarge.path = "/post_body_too_large";
    postBodyTooLarge.root = "./www/one/pages";
    postBodyTooLarge.index = "postBodyTooLarge.html";
    postBodyTooLarge.autoindex = false;
    postBodyTooLarge.allowedMethod = {"GET", "POST"};
    config.locations.push_back(postBodyTooLarge);

	// 404 file not found
	 // test chmod 000 with
    Location missing;
    missing.path = "/missing";
    missing.root = "./www";
    missing.index = "missing.html";
    missing.allowedMethod = {"GET"};
    config.locations.push_back(missing);

    //  403 when no index file: todo: got not mine 403..
    Location files;
    files.path = "/files";
    files.root = "./www/files";
    files.autoindex = false;
    files.allowedMethod = {"GET"};
    config.locations.push_back(files);  

    // expect to see directory listing of www/files with autoindex on
    Location files_auto;
    files_auto.path = "/files_auto";
    files_auto.root = "./www/files";
    files_auto.autoindex = true;
    files_auto.allowedMethod = {"GET"};
    config.locations.push_back(files_auto);

    // test JSON API
    Location api;
    api.path = "/api/data_json";
    api.root = "./www/api";
    api.index = "data.json";
    api.autoindex = false;
    api.allowedMethod = {"GET"};
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