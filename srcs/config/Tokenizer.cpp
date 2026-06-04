#include "../../includes/config/Parser.hpp"

#include <cctype>

/*
 * Tokenize config file into Tokens.
 * Loops through the input character by character, while pushing tokens into a vector.
 */
std::vector<Token> Tokenizer::tokenize() {
	std::vector<Token> tokens;

	while (_index < _input.size()) {
		skipWhitespaceAndComments();
		if (_index >= _input.size()) {
			break;
		}

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

	tokens.push_back(Token{TOKEN_EOF, "", _line, _column});
	return (tokens);
}

// It skips over whitespaces and comments...
void Tokenizer::skipWhitespaceAndComments() {
	while (_index < _input.size()) {
		char current = _input[_index];
		// Skip whitespaces
		if (std::isspace(static_cast<unsigned char>(current))) {
			advance();
			continue;
		}

		// Skip comments (#)
		if (current == '#') {
			while (_index < _input.size() && _input[_index] != '\n')
				advance();
			continue;
		}
		break;
	}
}

// Create a token out of single-characters
Token Tokenizer::makeSingle(TokenType type) const {
	return (Token{type, "", _line, _column});
}

// Create a token out of multiple characters
Token Tokenizer::readWord() {
	std::size_t startLine = _line;
	std::size_t startCol = _column;
	std::string value;

	while (_index < _input.size()) {
		char current = _input[_index];
		// Close word
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

/*
 * Advance the current position in the input by one character.
 * Updates line and column numbers.
 */
void Tokenizer::advance() {
	if (_input[_index] == '\n') {
		++_line;
		_column = 1;
	} else {
		++_column;
	}
	++_index;
}
