#include "config/Config.hpp"
#include "config/Parser.hpp"
#include "config/Tokenizer.hpp"

#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

ServerConfig::ServerConfig() : listen(-1) {}

std::vector<Token> Tokenizer::tokenize() {
	std::vector<Token> tokens;

	// Loop through the input character by character
	while (_index < _input.size()) {
		skipWhitespaceAndComments();

		// Check if we've reached the end of the input after skipping whitespace/comments
		if (_index >= _input.size()) {
			break;
		}

		// Handle single-character tokens, or read a word
		char current = _input[_index];
		if (current == '{') {
			tokens.push_back(makeSingle(TOKEN_LBRACE));
			advance();
		} else if (current == '}') {
			tokens.push_back(makeSingle(TOKEN_RBRACE));
			advance();
		} else if (current == ';') {
			tokens.push_back(makeSingle(TOKEN_SEMICOLON));
			advance();
		} else {
			tokens.push_back(readWord());
		}
	}

	// Add an EOF token at the end of the token stream
	tokens.push_back(Token{TOKEN_EOF, "", _line, _column});
	return (tokens);
}

Token Tokenizer::makeSingle(TokenType type) const {
	return (Token{type, "", _line, _column});
}

void Tokenizer::advance() {
	if (_input[_index] == '\n') {
		++_line;
		_column = 1;
	} else {
		++_column;
	}
	++_index;
}

void Tokenizer::skipWhitespaceAndComments() {
	while (_index < _input.size()) {
		char current = _input[_index];
		// Skip whitespace characters
		if (std::isspace(static_cast<unsigned char>(current))) {
			advance();
			continue;
		}
		// Skip comments (starting with '#')
		if (current == '#') {
			while (_index < _input.size() && _input[_index] != '\n') {
				advance();
			}
			continue;
		}
		break;
	}
}

Token Tokenizer::readWord() {
	std::size_t startLine = _line;
	std::size_t startCol = _column;
	std::string value;

	while (_index < _input.size()) {
		char current = _input[_index];
		// Words are terminated by a whitespace or special characters
		if (std::isspace(static_cast<unsigned char>(current))
			|| current == '{'
			|| current == '}'
			|| current == ';'
			|| current == '#') {
			break;
		}
		value.push_back(current);
		advance();
	}

	if (value.empty()) {
		throw ParseError("unexpected token", startLine, startCol);
	}

	return (Token{TOKEN_WORD, value, startLine, startCol});
}

// Main parsing function, returns the fully parsed ConfigFile
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

// Look at the current token without consuming it
const Token& Parser::peek() const {
	return (_tokens[_index]);
}

// Look at the previous token (the one most recently consumed)
const Token& Parser::previous() const {
	return (_tokens[_index - 1]);
}

// Check if the current token matches the expected type
bool Parser::check(TokenType type) const {
	return (peek().type == type);
}

// Move to the next token and return the one before it
const Token& Parser::advance() {
	if (!check(TOKEN_EOF)) {
		++_index;
	}

	return (previous());
}

// Consume a token of the expected type, or throw an error if it doesn't match
const Token& Parser::consume(TokenType type, const std::string& message) {
	if (check(type)) {
		return advance();
	}

	throw ParseError(message, peek().line, peek().column);
}

// Consume a token that must match a specific word
const Token& Parser::consumeWord(const std::string& expected) {
	const Token& token = consume(TOKEN_WORD, "expected '" + expected + "'");
	if (token.value != expected) {
		throw ParseError("expected '" + expected + "'", token.line, token.column);
	}

	return (token);
}

// Parse a server block, which should start with 'server' and be enclosed in braces
ServerConfig Parser::parseServerBlock() {
	consumeWord("server");
	consume(TOKEN_LBRACE, "expected '{' after server");

	ServerConfig server;

	while (!check(TOKEN_RBRACE)) {
		if (check(TOKEN_EOF)) {
			throw ParseError("unterminated server block", peek().line, peek().column);
		}

		if (checkWord("location")) {
			server.locations.push_back(parseLocationBlock());
		} else {
			parseDirectiveInto(server.directives, &server);
		}
	}

	consume(TOKEN_RBRACE, "expected '}' after server block");
	validateServer(server);
	return (server);
}

// Parse a location block, which should start with 'location' followed by a path, and enclosed in braces
LocationConfig Parser::parseLocationBlock() {
	consumeWord("location");
	const Token& pathToken = consume(TOKEN_WORD, "expected path after location");
	consume(TOKEN_LBRACE, "expected '{' after location path");

	LocationConfig location;
	location.path = pathToken.value;

	while (!check(TOKEN_RBRACE)) {
		if (check(TOKEN_EOF)) {
			throw ParseError("unterminated location block", peek().line, peek().column);
		}
		parseDirectiveInto(location.directives, static_cast<ServerConfig*>(0));
	}

	consume(TOKEN_RBRACE, "expected '}' after location block");
	return (location);
}

// Check if the current token is a word and matches the specified value
bool Parser::checkWord(const std::string& value) const {
	return (check(TOKEN_WORD) && peek().value == value);
}

// Parse a directive and store it in the provided directives map
// Assign known server fields if applicable
void Parser::parseDirectiveInto(
	std::map<std::string, std::vector<std::string> >& directives,
	ServerConfig* server) {
	const Token& key = consume(TOKEN_WORD, "expected directive name");
	std::vector<std::string> values;

	while (!check(TOKEN_SEMICOLON)) {
		if (check(TOKEN_EOF) || check(TOKEN_LBRACE) || check(TOKEN_RBRACE)) {
			throw ParseError("expected ';' after directive '" + key.value + "'", peek().line, peek().column);
		}
		values.push_back(consume(TOKEN_WORD, "expected directive value").value);
	}
	consume(TOKEN_SEMICOLON, "expected ';' after directive");

	directives[key.value] = values;
	if (server) {
		assignKnownServerFields(*server, key, values);
	}
}

// For known server directives, assign their values to the corresponding fields in the ServerConfig structure
void Parser::assignKnownServerFields(ServerConfig& server, const Token& key, const std::vector<std::string>& values) {
	if (key.value == "listen") {
		if (values.size() != 1 || !isUnsigned(values[0])) {
			throw ParseError("listen expects one numeric value", key.line, key.column);
		}
		server.listen = static_cast<int>(toUnsigned(values[0]));
	} else if (key.value == "host") {
		if (values.size() != 1) {
			throw ParseError("host expects one value", key.line, key.column);
		}
		server.host = values[0];
	} else if (key.value == "server_name") {
		if (values.size() != 1) {
			throw ParseError("server_name expects one value", key.line, key.column);
		}
		server.server_name = values[0];
	} else if (key.value == "client_max_body_size") {
		if (values.size() != 1 || !isUnsigned(values[0])) {
			throw ParseError("client_max_body_size expects one numeric value", key.line, key.column);
		}
	}
}

// Validate that the server block has all required directives and that location blocks have valid allowed_methods
void Parser::validateServer(const ServerConfig& server) {
	if (server.listen < 0) {
		throw ParseError("missing required directive 'listen'", peek().line, peek().column);
	}
	if (server.host.empty()) {
		throw ParseError("missing required directive 'host'", peek().line, peek().column);
	}
	if (server.server_name.empty()) {
		throw ParseError("missing required directive 'server_name'", peek().line, peek().column);
	}

	for (std::size_t i = 0; i < server.locations.size(); ++i) {
		validateLocation(server.locations[i]);
	}
}

// Validate that the allowed_methods directive in 'location' only contains valid HTTP methods
void Parser::validateLocation(const LocationConfig& location) {
	std::map<std::string, std::vector<std::string> >::const_iterator it = location.directives.find("allowed_methods");
	if (it == location.directives.end()) {
		return;
	}

	const std::vector<std::string>& methods = it->second;
	for (std::size_t i = 0; i < methods.size(); ++i) {
		if (methods[i] != "GET" && methods[i] != "POST" && methods[i] != "DELETE") {
			throw ParseError("invalid method in allowed_methods: " + methods[i], peek().line, peek().column);
		}
	}
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

// Convert a string to an unsigned long
unsigned long Parser::toUnsigned(const std::string& text) {
	std::istringstream iss(text);
	unsigned long value = 0;
	iss >> value;
	return (value);
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
