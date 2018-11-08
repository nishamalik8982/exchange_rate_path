#include "string_tokenizer.h"

StringTokenizer::StringTokenizer(const std::string& s) :
	m_string(s),
	m_token(),
	m_offset(0)
{
}

void StringTokenizer::reset()
{
	m_offset = 0;
	m_token.clear();
}

bool StringTokenizer::parseNextToken(const std::string& delimiters) 
{
	auto i = m_string.find_first_not_of(delimiters, m_offset);
	if (i == std::string::npos) {
		m_offset = m_string.length();
		return false;
	}

	auto j = m_string.find_first_of(delimiters, i);
	if (j == std::string::npos) {
		m_token = m_string.substr(i);
		m_offset = m_string.length();
		return true;
	}

	m_token = m_string.substr(i, j - i);
	m_offset = j;
	return true;
}
