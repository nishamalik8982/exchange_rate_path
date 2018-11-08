#ifndef STRING_TOKENIZER_H__
#define STRING_TOKENIZER_H__

#include <string>

// This class tokenize string using provided delimiters.
// Based on this public material:
// https://stackoverflow.com/a/53862/1540501
class StringTokenizer {
public:
	// Initializes StringTokenizer object with given string and delimiters.
	StringTokenizer(const std::string& s);

	// Reset parsing state.
	void reset();

	// Attempt to parse next token using provided delimiters.
	// Returns true if token parsed, otherwise returns false.
	bool parseNextToken(const std::string& delimiters);

	// Returns last parsed token
	const std::string& getToken() const noexcept
	{
		return m_token;
	}

private:
	// String to be parsed
	const std::string& m_string;

	// Last parsed token
	std::string m_token;

	// Current offset 
	std::size_t m_offset;
};

#endif // STRING_TOKENIZER_H__
