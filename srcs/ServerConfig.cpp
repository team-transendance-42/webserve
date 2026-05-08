#include "../includes/ServerConfig.hpp"

ServerConfig createDefaultServerConfig() {
    ServerConfig config;

    config.host = "localhost";
    config.port = 8080; // todo: 80 is for ./tester
    config.server_names.push_back("one");
    config.clientMaxBodySize= 1 * 1024 * 1024; //1MB for testing: 413 global policy 
    config.default_server = true;

    config.errorPages[400] = "./www/errors/400.html";
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
	 *  test not authorized access to /secret
	 *  because config says: denyAll = true 
     get 403: forbidden (e.g., /secret with denyAll=true, or chmod 000 on a file/dir)
	*/	
    Location secret;
    secret.path = "/secret";
    secret.root = "./www/one/secret";
    secret.index = "index.html";
    secret.autoindex = false;
    secret.allowedMethod = {"GET"};
    secret.denyAll = true;
    config.locations.push_back(secret);

    // test chmod 000 for root and/or root/index.html
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
	// test chmod 000 with: edge case, still need to get 404 if file is missing, even if dir is not accessible
    Location missing;
    missing.path = "/missing";
    missing.root = "./www";
    missing.index = "missing.html";
    missing.allowedMethod = {"GET"};
    config.locations.push_back(missing);

    //  403 when no index file
    Location files;
    files.path = "/files";
    files.root = "./www/files";
    files.autoindex = false;
    files.allowedMethod = {"GET"};
    config.locations.push_back(files);  

    // page with custom delete UI for files directory
    Location deleteFile;
    deleteFile.path = "/delete_create_file";
    deleteFile.root = "./www/files";
    deleteFile.index = "index.html";
    deleteFile.allowedMethod = {"GET", "POST", "DELETE"};
    deleteFile.upload_enabled = true; // create file for testing delete, not secure for production
    deleteFile.upload_path = "./www/files";
    config.locations.push_back(deleteFile);

    // directory listing with autoindex
    Location filesAuto;
    filesAuto.path = "/files_auto";
    filesAuto.root = "./www/files";
    filesAuto.autoindex = true;
    filesAuto.allowedMethod = {"GET"};
    config.locations.push_back(filesAuto);

    // test JSON API
    Location api;
    api.path = "/api/data_json";
    api.root = "./www/api";
    api.index = "data.json";
    api.autoindex = false;
    api.allowedMethod = {"GET"};
    config.locations.push_back(api);

    // test upload doc on the server
    // Location upload;
    // upload.path = "/upload";
    // upload.root = "./www/one";
    // upload.index = "upload.html";
    // upload.upload_enabled = true;
    // upload.upload_path = "./www/uploads";
    // upload.clientMaxBodySize = 5 * 1024 * 1024; // 5 MiB for uploads
    // upload.allowedMethod = {"GET", "POST"};
    // config.locations.push_back(upload);

    return config;
}

// Swap this call for parseConfigFile() once the parser is ready.
// Each entry maps to one server block in default.conf.
std::vector<ServerConfig> createDefaultServerConfigs() {
    std::vector<ServerConfig> configs;
    configs.push_back(createDefaultServerConfig()); // server 1 — port 8080

    // server 2 — API on port 8081 (mirrors default.conf server block 2)
    ServerConfig api;
    api.host = "127.0.0.1";
    api.port = 8081;
    api.server_names = {"api"};
    api.clientMaxBodySize = 524288; // 512 KB
    api.errorPages[404] = "./www/errors/404.html";
    api.errorPages[500] = "./www/errors/500.html";
    Location locApi;
    locApi.path = "/api/v1";
    locApi.root = "./www/api";
    locApi.index = "data.json";
    locApi.autoindex = false;
    locApi.allowedMethod = {"GET", "POST"};
    api.locations.push_back(locApi);
    Location locHealth;
    locHealth.path = "/api/health";
    locHealth.root = "./www/api";
    locHealth.index = "health.json";
    locHealth.autoindex = false;
    locHealth.allowedMethod = {"GET"};
    api.locations.push_back(locHealth);
    configs.push_back(api);

    // server 3 — static assets on port 8082 (mirrors default.conf server block 3)
    ServerConfig sta;
    sta.host = "127.0.0.1";
    sta.port = 8082;
    sta.server_names = {"static"};
    sta.clientMaxBodySize = 0;
    sta.errorPages[404] = "./www/errors/404.html";
    Location locStatic;
    locStatic.path = "/static";
    locStatic.root = "./www/static";
    locStatic.index = "index.html";
    locStatic.autoindex = false;
    locStatic.allowedMethod = {"GET"};
    sta.locations.push_back(locStatic);
    Location locDl;
    locDl.path = "/downloads";
    locDl.root = "./www/downloads";
    locDl.autoindex = true;
    locDl.allowedMethod = {"GET"};
    sta.locations.push_back(locDl);
    configs.push_back(sta);

    return configs;
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
    return (best);
}
