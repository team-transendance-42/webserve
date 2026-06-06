#include "../../includes/config/Parser.hpp"

#include <cctype>
#include <fstream>
#include <set>
#include <sstream>

/*
 * Main parse function.
 * Returns the config as a ConfigFile structure.
 * On error throws ParseError (with a message and line/column info).
 */
ConfigFile Parser::parseConfig() {
	ConfigFile result;

	while (!check(TOKEN_EOF)) {
		result.servers.push_back(parseServerBlock());
	}

	if (result.servers.empty()) {
		throw ParseError("no server blocks found", 1, 1);
	}

	return (result);
}

/*
 * Parse the server block.
 * Should start with 'server' followed by '{'
 * Then looks for directives/location blocks
 * Closes with a '}'
 *
 * On error throws ParseError (with message and line/column info).
 */
ServerConfig Parser::parseServerBlock() {
	consumeWord("server");
	consume(TOKEN_LBRACE, "expected '{' after server");

	ServerConfig server;
	std::set<std::string> seenDirectives;

	while (!check(TOKEN_RBRACE)) {
		if (check(TOKEN_EOF)) {
			throw ParseError("unterminated server block", peek().line, peek().column);
		}

		if (checkWord("location")) {
			server.locations.push_back(parseLocationBlock());
		} else {
			parseServerDirective(server, seenDirectives);
		}
	}

	consume(TOKEN_RBRACE, "expected '}' after server block");
	validateServer(server);
	return (server);
}

/*
 * Parse a location block.
 * Should start with 'location' followed by a path
 * Must be enclosed in '{' and '}'
 *
 * On error throws ParseError (with message and line/column info).
 */
Location Parser::parseLocationBlock() {
	consumeWord("location");
	const Token& pathToken = consume(TOKEN_WORD, "expected path after location");
	consume(TOKEN_LBRACE, "expected '{' after location path");

	Location location;
	std::set<std::string> seenDirectives;
	location.path = pathToken.value;

	while (!check(TOKEN_RBRACE)) {
		if (check(TOKEN_EOF)) {
			throw ParseError("unterminated location block", peek().line, peek().column);
		}
		parseLocationDirective(location, seenDirectives);
	}

	consume(TOKEN_RBRACE, "expected '}' after location block");
	return (location);
}

// Parse a server directive and assign known fields
void Parser::parseServerDirective(ServerConfig& server, std::set<std::string>& seenDirectives) {
	const Token& key = consume(TOKEN_WORD, "expected directive name");
	std::vector<std::string> values;

	while (!check(TOKEN_SEMICOLON)) { // Read until ';'
		if (check(TOKEN_EOF) || check(TOKEN_LBRACE) || check(TOKEN_RBRACE)) {
			throw ParseError("expected ';' after directive '" + key.value + "'", peek().line, peek().column);
		}
		values.push_back(consume(TOKEN_WORD, "expected directive value").value);
	}
	consume(TOKEN_SEMICOLON, "expected ';' after directive");

	if (key.value == "error_page") {
		// include error code to allow multiple error_page directives with different codes
		if (seenDirectives.count(key.value + *(values.end() - 2)) > 0) {
			throw ParseError("duplicate server directive: " + key.value + " " + *(values.end() - 2), key.line, key.column);
		}
		seenDirectives.insert(key.value + *(values.end() - 2));
	// Throw error if a duplicate directive is found in one location block
	} else {
		if (seenDirectives.count(key.value) > 0) {
			throw ParseError("duplicate server directive: " + key.value, key.line, key.column);
		}
		seenDirectives.insert(key.value);
	}

	assignKnownServerFields(server, key, values);
}

// Parse a location directive and assign known fields
void Parser::parseLocationDirective(Location& location, std::set<std::string>& seenDirectives) {
	const Token& key = consume(TOKEN_WORD, "expected directive name");
	std::vector<std::string> values;

	while (!check(TOKEN_SEMICOLON)) { // Read until ';'
		if (check(TOKEN_EOF) || check(TOKEN_LBRACE) || check(TOKEN_RBRACE)) {
			throw ParseError("expected ';' after directive '" + key.value + "'", peek().line, peek().column);
		}
		values.push_back(consume(TOKEN_WORD, "expected directive value").value);
	}
	consume(TOKEN_SEMICOLON, "expected ';' after directive");

	// Throw error if a duplicate directive is found in one location block
	if (seenDirectives.count(key.value) > 0) {
		throw ParseError("duplicate location directive: " + key.value, key.line, key.column);
	}
	seenDirectives.insert(key.value);

	assignKnownLocationFields(location, key, values);
}

// For known server directives, assign their values to the fields in ServerConfig
void Parser::assignKnownServerFields(ServerConfig& server, const Token& key, const std::vector<std::string>& values) {
	if (key.value == "listen") {
		if (values.size() != 1 || !isUnsigned(values[0])) {
			throw ParseError("listen expects one numeric value", key.line, key.column);
		}
		server.port = std::stoul(values[0]);
	} else if (key.value == "host") {
		if (values.size() != 1) {
			throw ParseError("host expects one value", key.line, key.column);
		}
		server.host = values[0];
	} else if (key.value == "server_name") {
		if (values.empty()) {
			throw ParseError("server_name expects at least one value", key.line, key.column);
		}
		server.server_names = values;
	} else if (key.value == "clientMaxBodySize") {
		if (values.size() != 1 || !isUnsigned(values[0])) {
			throw ParseError("clientMaxBodySize expects one numeric value", key.line, key.column);
		}
		server.clientMaxBodySize = std::stoul(values[0]);
	} else if (key.value == "error_page") {
		if (values.size() != 2 || !isUnsigned(values[0])) {
			throw ParseError("error_page expects: <code> <path>", key.line, key.column);
		}
		server.errorPages[std::stoul(values[0])] = values[1];
	} else if (key.value == "default_server") {
		if (values.empty()) {
			server.default_server = true;
		} else if (values.size() == 1 && (values[0] == "on" || values[0] == "off")) {
			server.default_server = (values[0] == "on");
		} else {
			throw ParseError("default_server expects no value or one of: on|off", key.line, key.column);
		}
	} else {
		throw ParseError("unknown server directive: " + key.value, key.line, key.column);
	}
}

// For known location directives, assign their values to the fields in Location
void Parser::assignKnownLocationFields(Location& location, const Token& key, const std::vector<std::string>& values) {
	if (key.value == "root") {
		if (values.size() != 1) {
			throw ParseError("root expects one value", key.line, key.column);
		}
		location.root = values[0];
	} else if (key.value == "index") {
		if (values.size() != 1) {
			throw ParseError("index expects one value", key.line, key.column);
		}
		location.index = values[0];
	} else if (key.value == "autoindex") {
		if (values.size() != 1 || (values[0] != "on" && values[0] != "off")) {
			throw ParseError("autoindex expects one value: on|off", key.line, key.column);
		}
		location.autoindex = (values[0] == "on");
	} else if (key.value == "deny_all") {
		if (values.size() != 1 || (values[0] != "on" && values[0] != "off")) {
			throw ParseError("deny_all expects one value: on|off", key.line, key.column);
		}
		location.denyAll = (values[0] == "on");
	} else if (key.value == "allowedMethod") {
		if (values.empty()) {
			throw ParseError("allowedMethod expects at least one method", key.line, key.column);
		}
		location.allowedMethod = values;
	} else if (key.value == "return") {
		if (values.size() != 2 || !isUnsigned(values[0])) {
			throw ParseError("return expects: <code> <url>", key.line, key.column);
		}
		location.redirect_code = std::stoul(values[0]);
		location.redirect_url = values[1];
	} else if (key.value == "clientMaxBodySize") {
		if (values.size() != 1 || !isUnsigned(values[0])) {
			throw ParseError("clientMaxBodySize expects one numeric value", key.line, key.column);
		}
		location.clientMaxBodySize = std::stoul(values[0]);
	} else if (key.value == "cgi_extension") {
		if (values.size() != 1) {
			throw ParseError("cgi_extension expects one value", key.line, key.column);
		}
		location.cgi_extension = values[0];
	} else if (key.value == "cgi_pass") {
		if (values.size() != 1) {
			throw ParseError("cgi_pass expects one value", key.line, key.column);
		}
		location.cgi_pass = values[0];
	} else if (key.value == "upload_path") {
		if (values.size() != 1) {
			throw ParseError("upload_path expects one value", key.line, key.column);
		}
		location.upload_path = values[0];
	} else if (key.value == "deny") {
		if (values.size() != 1 || values[0] != "all") {
			throw ParseError("deny expects one value: all", key.line, key.column);
		}
		location.denyAll = true;
	} else {
		throw ParseError("unknown location directive: " + key.value, key.line, key.column);
	}
}

/*
 * Validate the server blcok for all required directives.
 * & location blocks for valid AllowedMethods.
 */
void Parser::validateServer(const ServerConfig& server) {
	if (server.port < 0) {
		throw ParseError("missing required directive 'listen'", peek().line, peek().column);
	}
	if (server.server_names.empty()) {
		throw ParseError("missing required directive 'server_name'", peek().line, peek().column);
	}

	for (std::size_t i = 0; i < server.locations.size(); ++i) {
		validateLocation(server.locations[i]);
	}
}

// Validate the allowedMethods directive in 'location'
void Parser::validateLocation(const Location& location) {
	if (!location.cgi_extension.empty() && location.cgi_extension[0] != '.') {
		throw ParseError("cgi_extension must start with '.'", peek().line, peek().column);
	}

	if (location.cgi_extension.empty() != location.cgi_pass.empty()) {
		throw ParseError("cgi_extension and cgi_pass must be set together", peek().line, peek().column);
	}

	if (location.allowedMethod.empty()) {
		return;
	}

	const std::vector<std::string>& methods = location.allowedMethod;
	for (std::size_t i = 0; i < methods.size(); ++i) {
		if (methods[i] != "GET" && methods[i] != "POST" && methods[i] != "DELETE") {
			throw ParseError("invalid method in allowedMethods: " + methods[i], peek().line, peek().column);
		}
	}
}

// Look at the current token without consuming it
const Token& Parser::peek() const {
	return (_tokens[_index]);
}

// Look at the previous token (the one most recently consumed)
const Token& Parser::previous() const {
	return (_tokens[_index - 1]);
}

// Return the current token, and move to the next one
const Token& Parser::advance() {
	if (!check(TOKEN_EOF)) {
		++_index;
	}

	return (previous());
}

// Consume the token if it matches the given type
const Token& Parser::consume(TokenType type, const std::string& message) {
	if (check(type)) {
		return advance();
	}

	throw ParseError(message, peek().line, peek().column);
}

// Consume the token if it is a word AND matches the given value
const Token& Parser::consumeWord(const std::string& expected) {
	const Token& token = consume(TOKEN_WORD, "expected '" + expected + "'");
	if (token.value != expected) {
		throw ParseError("expected '" + expected + "'", token.line, token.column);
	}

	return (token);
}

// Check if the current token is a word and matches given value
bool Parser::checkWord(const std::string& value) const {
	return (check(TOKEN_WORD) && peek().value == value);
}

// Check if the current token matches the expected type
bool Parser::check(TokenType type) const {
	return (peek().type == type);
}

// Check if a string represents an unsigned int
bool Parser::isUnsigned(const std::string& text) {
	if (text.empty()) {
		return (false);
	}
	for (std::size_t i = 0; i < text.size(); ++i) {
		if (!std::isdigit(static_cast<unsigned char>(text[i]))) {
			return (false);
		}
	}
	return (true);
}

// Read an entire file into a string
std::string readAll(const std::string& filePath) {
    std::ifstream file(filePath.c_str());
    if (!file) {
        throw std::runtime_error("failed to open config file: " + filePath);
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return (buffer.str());
}

// Read config file and return a ConfigFile structure
ConfigFile ConfigParser::parseFile(const std::string& filePath) const {
    return (parseString(readAll(filePath)));
}

// Parse a config from a string
ConfigFile ConfigParser::parseString(const std::string& text) const {
    Tokenizer tokenizer(text);
    std::vector<Token> tokens = tokenizer.tokenize();
    Parser parser(tokens);
    return (parser.parseConfig());
}
