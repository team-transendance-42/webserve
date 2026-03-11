
#ifndef PARSER_HPP
#define PARSER_HPP

#include "Config.hpp"
#include "Tokenizer.hpp"
#include <stdexcept>
#include <sstream>

// Exception type for parse errors, includes line and column information
class ParseError : public std::runtime_error {
private:
    static std::string buildMessage(const std::string& message, std::size_t line, std::size_t column) {
        std::ostringstream oss;
        oss << "Config parse error at line " << line << ", column " << column << ": " << message;
        return (oss.str());
    }

public:
    ParseError(const std::string& message, std::size_t line, std::size_t column) : std::runtime_error(buildMessage(message, line, column)) {}
};

class Parser {
private:
    const std::vector<Token>& _tokens;
    std::size_t               _index;

	// Look at the current token without consuming it
    const Token& peek() const;

	// Look at the previous token (the one most recently consumed)
    const Token& previous() const;

	// Check if the current token matches the expected type
    bool check(TokenType type) const;

	// Move to the next token and return the one before it
    const Token& advance();

	// Consume a token of the expected type, or throw an error if it doesn't match
    const Token& consume(TokenType type, const std::string& message);

	// Consume a token that must match a specific word
    const Token& consumeWord(const std::string& expected);

	// Parse a server block, which should start with 'server' and be enclosed in braces
    ServerConfig parseServerBlock();

	// Parse a location block, which should start with 'location' followed by a path, and enclosed in braces
    LocationConfig parseLocationBlock();

	// Check if the current token is a word and matches the specified value
    bool checkWord(const std::string& value) const;

	// Parse a directive and store it in the provided directives map
	// Assign known server fields if applicable
    void parseDirectiveInto(
        std::map<std::string, std::vector<std::string> >& directives,
        ServerConfig* server);

	// For known server directives, assign their values to the corresponding fields in the ServerConfig structure
    void assignKnownServerFields(ServerConfig& server, const Token& key, const std::vector<std::string>& values);

	// Validate that the server block has all required directives and that location blocks have valid allowed_methods
    void validateServer(const ServerConfig& server);

	// Validate that the allowed_methods directive in 'location' only contains valid HTTP methods
    void validateLocation(const LocationConfig& location);

	// Check if a string represents an unsigned int
    static bool isUnsigned(const std::string& text);

	// Convert a string to an unsigned long
    static unsigned long toUnsigned(const std::string& text);

public:
    explicit Parser(const std::vector<Token>& tokens) : _tokens(tokens), _index(0) {}

	// Main parsing function, returns the fully parsed ConfigFile
    ConfigFile parseConfig();
};

#endif // PARSER_HPP
