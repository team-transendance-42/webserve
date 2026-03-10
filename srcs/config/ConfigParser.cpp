#include "ConfigParser.hpp"

#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

ServerConfig::ServerConfig() : listen(-1) {}

namespace {

// Resulting token types for the tokenizer
enum TokenType {
    TOKEN_WORD,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_SEMICOLON,
    TOKEN_EOF
};

// A single token produced by the tokenizer
struct Token {
    TokenType    type;
    std::string  value;
    std::size_t  line;
    std::size_t  column;
};

// Exception type for parse errors, includes line and column information
class ParseError : public std::runtime_error {
public:
    ParseError(const std::string& message, std::size_t line, std::size_t column)
        : std::runtime_error(buildMessage(message, line, column)) {}

private:
    static std::string buildMessage(const std::string& message, std::size_t line, std::size_t column) {
        std::ostringstream oss;
        oss << "Config parse error at line " << line << ", column " << column << ": " << message;
        return (oss.str());
    }
};

// Convert input text into a stream of tokens
class Tokenizer {
public:
    explicit Tokenizer(const std::string& input)
        : _input(input), _index(0), _line(1), _column(1) {}

	// Main tokenization loop
    std::vector<Token> tokenize() {
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

private:
    const std::string& _input;
    std::size_t        _index;
    std::size_t        _line;
    std::size_t        _column;

	// Create a token for single-character tokens
    Token makeSingle(TokenType type) const {
        return (Token{type, "", _line, _column});
    }

	// Advance the current position in the input
	// Update line and column numbers accordingly
    void advance() {
        if (_input[_index] == '\n') {
            ++_line;
            _column = 1;
        } else {
            ++_column;
        }
        ++_index;
    }

	// Skip over whitespace and comments in the input
    void skipWhitespaceAndComments() {
        while (_index < _input.size()) {
            char current = _input[_index];
			// Skip whitespace characters
            if (std::isspace(static_cast<unsigned char>(current))) {
                advance();
                continue;
            }
			// Skip comments starting with '#'
            if (current == '#') {
                while (_index < _input.size() && _input[_index] != '\n') {
                    advance();
                }
                continue;
            }
            break;
        }
    }

	// Read one word from the input
    Token readWord() {
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
};

// Parse the token stream into a ConfigFile structure
class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens) : _tokens(tokens), _index(0) {}

	// Main parsing function, returns the fully parsed ConfigFile
    ConfigFile parseConfig() {
        ConfigFile result;

        while (!check(TOKEN_EOF)) {
            result.servers.push_back(parseServerBlock());
        }

        if (result.servers.empty()) {
            throw ParseError("no server blocks found", 1, 1);
        }

        return (result);
    }

private:
    const std::vector<Token>& _tokens;
    std::size_t               _index;

	// Look at the current token without consuming it
    const Token& peek() const {
        return (_tokens[_index]);
    }

	// Look at the previous token (the one most recently consumed)
    const Token& previous() const {
        return (_tokens[_index - 1]);
    }

	// Check if the current token matches the expected type
    bool check(TokenType type) const {
        return (peek().type == type);
    }

	// Move to the next token and return the one before it
    const Token& advance() {
        if (!check(TOKEN_EOF)) {
            ++_index;
        }

        return (previous());
    }

	// Consume a token of the expected type, or throw an error if it doesn't match
    const Token& consume(TokenType type, const std::string& message) {
        if (check(type)) {
            return advance();
        }

        throw ParseError(message, peek().line, peek().column);
    }

	// Consume a token that must match a specific word
    const Token& consumeWord(const std::string& expected) {
        const Token& token = consume(TOKEN_WORD, "expected '" + expected + "'");
        if (token.value != expected) {
            throw ParseError("expected '" + expected + "'", token.line, token.column);
        }

        return (token);
    }

	// Parse a server block, which should start with 'server' and be enclosed in braces
    ServerConfig parseServerBlock() {
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
    LocationConfig parseLocationBlock() {
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
    bool checkWord(const std::string& value) const {
        return (check(TOKEN_WORD) && peek().value == value);
    }

	// Parse a directive and store it in the provided directives map
	// Assign known server fields if applicable
    void parseDirectiveInto(
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

    void assignKnownServerFields(ServerConfig& server, const Token& key, const std::vector<std::string>& values) {
        if (key.value == "listen") {
            if (values.size() != 1 || !isUnsigned(values[0])) {
                throw ParseError("listen expects one numeric value", key.line, key.column);
            }
            server.listen = static_cast<int>(toUnsigned(values[0]));
        }
        else if (key.value == "host") {
            if (values.size() != 1) {
                throw ParseError("host expects one value", key.line, key.column);
            }
            server.host = values[0];
        }
        else if (key.value == "server_name") {
            if (values.size() != 1) {
                throw ParseError("server_name expects one value", key.line, key.column);
            }
            server.server_name = values[0];
        }
        else if (key.value == "client_max_body_size") {
            if (values.size() != 1 || !isUnsigned(values[0])) {
                throw ParseError("client_max_body_size expects one numeric value", key.line, key.column);
            }
        }
    }

    void validateServer(const ServerConfig& server) {
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

    void validateLocation(const LocationConfig& location) {
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

    static bool isUnsigned(const std::string& text) {
        if (text.empty()) {
            return false;
        }
        for (std::size_t i = 0; i < text.size(); ++i) {
            if (!std::isdigit(static_cast<unsigned char>(text[i]))) {
                return false;
            }
        }
        return true;
    }

    static unsigned long toUnsigned(const std::string& text) {
        std::istringstream iss(text);
        unsigned long value = 0;
        iss >> value;
        return value;
    }
};

std::string readAll(const std::string& filePath) {
    std::ifstream file(filePath.c_str());
    if (!file) {
        throw std::runtime_error("failed to open config file: " + filePath);
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

} // namespace

ConfigFile ConfigParser::parseFile(const std::string& filePath) const {
    return parseString(readAll(filePath));
}

ConfigFile ConfigParser::parseString(const std::string& text) const {
    Tokenizer tokenizer(text);
    std::vector<Token> tokens = tokenizer.tokenize();
    Parser parser(tokens);
    return parser.parseConfig();
}
