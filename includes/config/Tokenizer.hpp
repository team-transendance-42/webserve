
#ifndef TOKENIZER_HPP
#define TOKENIZER_HPP

#include <string>
#include <vector>

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

// Convert input text into a stream of tokens
class Tokenizer {
private:
    const std::string& _input;
    std::size_t        _index;
    std::size_t        _line;
    std::size_t        _column;

	// Create a token for single-character tokens
    Token makeSingle(TokenType type) const;

	// Advance the current position in the input
	// Update line and column numbers accordingly
    void advance();

	// Skip over whitespace and comments in the input
    void skipWhitespaceAndComments();

	// Read one word from the input
    Token readWord();

public:
    explicit Tokenizer(const std::string& input) : _input(input), _index(0), _line(1), _column(1) {}

	// Main tokenization loop
    std::vector<Token> tokenize();
};

#endif // TOKENIZER_HPP
