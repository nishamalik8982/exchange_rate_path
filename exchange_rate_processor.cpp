#include "exchange_rate_processor.h"
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

namespace {
	const std::string TOKEN_DELIMITERS = " \t\v\r\n";
	const std::string EXCHANGE_RATE_REQUEST = "EXCHANGE_RATE_REQUEST";
	const std::string BEST_RATES_BEGIN = "BEST_RATES_BEGIN"; 
	const std::string BEST_RATES_END = "BEST_RATES_END"; 
	constexpr std::size_t INVALID_INDEX = std::numeric_limits<std::size_t>::max();
}

ExchangeRateProcessor::ExchangeRateProcessor() :
	m_exchanges(),
	m_currencies(),
	m_exchangeCurrencyPairToIndexMapping(),
	m_exchangeCurrencyPairsByIndex()
{
}

void ExchangeRateProcessor::processData(const std::string& data)
{
	try {
		// Try to get first token. If we can't do that,
		// report error and stop further processing.
		StringTokenizer t(data);
		if (!t.parseNextToken(TOKEN_DELIMITERS)) {
			throw std::invalid_argument("could not get first token");
		}
		const auto token = t.getToken();

		// Determine whether this is rate information or request
		// and route data to approproate processing function.
		if (token == EXCHANGE_RATE_REQUEST) {
			processRequest(t);
		} else {
			// We will need to parse info from scratch, so reset tokenizer
			t.reset();
			processInfo(t);
		}
	} catch (std::exception& ex) {
		// Report error if data was not properly processed
		std::cerr << "Error: " << ex.what() << ", data (" << data << ")" << std::endl;
	}
}

void ExchangeRateProcessor::processInfo(StringTokenizer& data)
{
	// Parse fields of rate info
	const auto timestamp = parseTimestampField(data, "timestamp");
	const auto exchange = parseStringField(data, "exchange");
	const auto sourceCurrency = parseStringField(data, "source_currency");
	const auto destinationCurrency = parseStringField(data, "destination_currency");
	const auto forwardFactor = parseDoubleField(data, "forward_factor");
	const auto backwardFactor = parseDoubleField(data, "backward_factor");

	// Validate forward and backward factors
	if (forwardFactor <= 0.0) {
		throw std::invalid_argument("invalid forward factor");
	}
	if (backwardFactor <= 0.0) {
		throw std::invalid_argument("invalid backward factor");
	}
	if (forwardFactor * backwardFactor > 1.0) {
		throw std::invalid_argument("invalid combination of forward and backward factors");
	}

	// Update graph adjacency table

	// Attempt to add exchanges and currencies, as result also determine
	// whether they are already known or not
	const auto pibExchange = m_exchanges.insert(exchange);
	const auto pibSourceCurrency = m_currencies.insert(sourceCurrency);
	const auto pibDestinationCurrency = m_currencies.insert(destinationCurrency);

	if (pibSourceCurrency.second) {
		// Source currency is brand new currency
		provisionNewCurrency(sourceCurrency);
	} else if (pibExchange.second) {
		// Source currency is already known, but this is new exchange,
		// so provision source currency for it
		provisionCurrencyForExchange(sourceCurrency, exchange);
	}
	if (pibDestinationCurrency.second) {
		// Destination currency is brand new currency
		provisionNewCurrency(destinationCurrency);
	} else if (pibExchange.second) {
		// Destination currency is already known, but this is new exchange,
		// so provision destination currency for it
		provisionCurrencyForExchange(destinationCurrency, exchange);
	}

	// Find or add exchange/currency pairs
	const auto sourceIndex = findExchangeCurrencyPair(exchange, sourceCurrency);
	const auto destinationIndex = findExchangeCurrencyPair(exchange, destinationCurrency); 

	// Update source -> destination edge
	auto& sourceToDestinationCell = m_adjacencyTable[sourceIndex][destinationIndex];
	if (sourceToDestinationCell.first < timestamp) {
		sourceToDestinationCell.first = timestamp;
		sourceToDestinationCell.second = forwardFactor;
	}

	// Update destination -> source edge
	auto& destinationToSourceCell = m_adjacencyTable[destinationIndex][sourceIndex];
	if (destinationToSourceCell.first < timestamp) {
		destinationToSourceCell.first = timestamp;
		destinationToSourceCell.second = backwardFactor;
	}
}

void ExchangeRateProcessor::processRequest(StringTokenizer& data)
{
	// Parse and validate fields of rate request
	const auto sourceExchange = parseStringField(data, "source_exchange");
	const auto sourceCurrency = parseStringField(data, "source_currency");
	const auto destinationExchange = parseStringField(data, "destination_exchange");
	const auto destinationCurrency = parseStringField(data, "destination_currency");

	// Locate source exchnage/currency pair
	const auto sourceIndex = findExchangeCurrencyPair(sourceExchange, sourceCurrency);
	if (sourceIndex == INVALID_INDEX) {
		printPath(sourceExchange, sourceCurrency, destinationExchange,
			destinationCurrency, nullptr);
		std::ostringstream err;
		err << "source currency/exchnage pair " << sourceExchange << "/" 
			<< sourceCurrency << " is unknown";
		throw std::invalid_argument(err.str());
	}

	// Locate destination exchnage/currency pair
	const auto destinationIndex = findExchangeCurrencyPair(destinationExchange,
		destinationCurrency);
	if (destinationIndex == INVALID_INDEX) {
		printPath(sourceExchange, sourceCurrency, destinationExchange,
			destinationCurrency, nullptr);
		std::ostringstream err;
		err << "destination currency/exchnage pair " << destinationExchange << "/"
			<< destinationCurrency << " is unknown";
		throw std::invalid_argument(err.str());
	}

	// Find best rate exchange path and print it out.
	auto path = generateExchangePath(sourceIndex, destinationIndex);
	printPath(sourceExchange, sourceCurrency, destinationExchange, destinationCurrency, &path);
}

std::string ExchangeRateProcessor::parseStringField(StringTokenizer& data, const char* fieldName)
{
	// Try read next token
	if (!data.parseNextToken(TOKEN_DELIMITERS)) {
		throw std::invalid_argument(std::string("missing ") + fieldName);
	}
	const auto& value = data.getToken();

	// Check that token is not empty
	if (value.empty()) {
		throw std::invalid_argument(std::string("empty ") + fieldName);
	}

	return value;
}

std::time_t ExchangeRateProcessor::parseTimestampField(StringTokenizer& data, const char* fieldName)
{
	// Read timestamp as string and validate its length
	auto timestamp = parseStringField(data, fieldName);
	constexpr std::size_t TIME_STRING_LENGTH = 25;
	if (timestamp.length() != TIME_STRING_LENGTH) {
		throw std::invalid_argument(std::string("invalid length of time field ") + fieldName);
	}

	// Extract timezone sign
	const auto tzSign = timestamp[19];
	if (!(tzSign == '-' || tzSign == '+')) {
		throw std::invalid_argument(std::string("invalid value of time field ") + fieldName);
	}

	// Extract time zone
	auto timeZone = timestamp.substr(20);
	timestamp.erase(timestamp.begin() + 19, timestamp.end());

	// Parse timestamp
	std::tm tmbTimestamp;
	memset(&tmbTimestamp, 0, sizeof(tmbTimestamp));
	tmbTimestamp.tm_isdst = 0;
	std::istringstream timestampInputStream(timestamp);
	timestampInputStream >> std::get_time(&tmbTimestamp, "%Y-%m-%dT%H:%M:%S");
	if (timestampInputStream.fail()) {
		throw std::invalid_argument(std::string("invalid date or time in time field ") + fieldName);
	}

	// Parse time zone
	std::tm tmbTimeZone;
	memset(&tmbTimeZone, 0, sizeof(tmbTimeZone));
	std::istringstream timeZoneInputStream(timeZone);
	timeZoneInputStream >> std::get_time(&tmbTimeZone, "%H:%M");
	if (timestampInputStream.fail()) {
		throw std::invalid_argument(std::string("invalid time zone in time field ") + fieldName);
	}

	// Create GMT time
	auto t = std::mktime(&tmbTimestamp);
	// Apply correction for time zone and return result
	const int tzOffset = (tzSign == '+' ? -1 : +1) 
		* (tmbTimeZone.tm_min + 60 * tmbTimeZone.tm_hour) * 60;
	return t + tzOffset;
}

double ExchangeRateProcessor::parseDoubleField(StringTokenizer& data, const char* fieldName)
{
	const auto s = parseStringField(data, fieldName);
	try {
		std::size_t pos = 0;
		const auto v = std::stod(s, &pos);
		if (pos < s.length()) {
			throw std::invalid_argument("invalid floating-point number");
		}
		return v;
	} catch (std::exception& ex) {
		std::ostringstream err;
		err << "invalid value of " << fieldName << ": " << ex.what();
		throw std::invalid_argument(err.str());
	}
}

std::size_t ExchangeRateProcessor::addExchangeCurrencyPair(const std::string& exchange,
	const std::string& currency)
{
	// Try to insert
	auto pair = std::make_pair(exchange, currency); 
	const auto pib = m_exchangeCurrencyPairToIndexMapping.insert(std::make_pair(pair,
		m_exchangeCurrencyPairsByIndex.size()));
	if (pib.second) {
		// This is new pair
		m_exchangeCurrencyPairsByIndex.push_back(std::move(pair));
		extendAdjancencyTable();
	}
	return pib.first->second;
}

std::size_t ExchangeRateProcessor::findExchangeCurrencyPair(const std::string& exchange,
	const std::string& currency) const
{
	auto it = m_exchangeCurrencyPairToIndexMapping.find(std::make_pair(exchange, currency));
	return it == m_exchangeCurrencyPairToIndexMapping.end() 
		? std::numeric_limits<std::size_t>::max()
		: it->second;
}

void ExchangeRateProcessor::extendAdjancencyTable()
{
	// Add extra column to all existing rows 
	for (auto& row: m_adjacencyTable) {
		row.push_back(std::pair<std::time_t, double>());
	}

	// Add new row
	m_adjacencyTable.push_back(std::vector<std::pair<std::time_t, double>>(
		m_exchangeCurrencyPairsByIndex.size()));
}

void ExchangeRateProcessor::provisionNewCurrency(const std::string& currency)
{
	// Enumerate all exchanges
	for (auto it1 = m_exchanges.cbegin(); it1 != m_exchanges.cend(); ++it1) {
		// Add currency for current exchange
		const auto& exchange1 = *it1;
		addExchangeCurrencyPair(exchange1, currency);
	}

	// Enumerate all exchanges
	for (auto it1 = m_exchanges.cbegin(); it1 != m_exchanges.cend(); ++it1) {
		// Add currency for current exchange
		const auto& exchange1 = *it1;
		const auto index1 = findExchangeCurrencyPair(exchange1, currency);
		// Enumerate "other" exchanges starting from next one to current
		auto it = it1;
		for (auto it2 = ++it; it2 != m_exchanges.cend(); ++it2) {
			const auto& exchange2 = *it2;
			const auto index2 = findExchangeCurrencyPair(exchange2, currency);
			// Add edges between same currcies on current and "other" exchanges
			const auto cellValue = std::make_pair(std::time(nullptr), 1.0);
			m_adjacencyTable[index1][index2] = cellValue;
			m_adjacencyTable[index2][index1] = cellValue;
		}
	}
}

void ExchangeRateProcessor::provisionCurrencyForExchange(const std::string& currency,
	const std::string& exchange)
{
	// Add currency for current exchange
	const auto index1 = addExchangeCurrencyPair(exchange, currency);

	// Enumerate "other" exchanges
	for (const auto& otherExchange: m_exchanges) {
		if (otherExchange != exchange) {
			const auto index2 = findExchangeCurrencyPair(otherExchange, currency);
			// Add edges between same currcies on current and "other" exchanges
			const auto cellValue = std::make_pair(std::time(nullptr), 1.0);
			m_adjacencyTable[index1][index2] = cellValue;
			m_adjacencyTable[index2][index1] = cellValue;
		}
	}
}

std::vector<std::size_t> ExchangeRateProcessor::generateExchangePath(const std::size_t sourceIndex,
	const std::size_t destinationIndex) const
{
	// Lookup table of edge weights initialized to 0 for each
	// (source_vertex, destination_vertex) pair
	std::vector<std::vector<double>> rate;
	const auto n = m_adjacencyTable.size();
	for (std::size_t i = 0; i < n; ++i) {
		rate.push_back(std::vector<double>(n));
	}

	// Lookup table of vertices initialized to INVALID_INDEX for each
	// (source_vertex, destination_vertex) pair.
	std::vector<std::vector<std::size_t>> next;
	for (std::size_t i = 0; i < n; ++i) {
		next.push_back(std::vector<std::size_t>(n, INVALID_INDEX));
	}

	// Fill "rate" and "next"
	for (std::size_t i = 0; i < n; ++i) {
		const auto& sourceRow = m_adjacencyTable[i];
		auto& destinationRow = rate[i];
		auto& nextRow = next[i];
		for (std::size_t j = 0; j < n; ++j) {
			const auto& sourceCell = sourceRow[j];
			// nonzero timestamp in 'first' is indicator of edge presence
			if (sourceCell.first) {
				destinationRow[j] = sourceCell.second;
				nextRow[j] = j;
			}
		}
	}

	// Modified Floyd-Warsall implementation
	for (std::size_t k = 0; k < n; ++k) {
		auto& rateKRow = rate[k];
		for (std::size_t i = 0; i < n; ++i) {
			auto& rateRow = rate[i];
			auto& nextRow = next[i];
			for (std::size_t j = 0; j < n; ++j) {
				auto& r = rateRow[j];
				const auto newR = rateRow[k] * rateKRow[j];
				if (r < newR) {
					r = newR;
					nextRow[j] = nextRow[k];
				}
			}
		}
	}

#ifdef _DEBUG
	std::cerr << std::endl << "Finding path from " << sourceIndex << " to " << destinationIndex << std::endl;
	printAdjacencyTable();
	printFloydWarsallTables(rate, next);
#endif

	// Construct path
	std::vector<std::size_t> path;
	std::unordered_set<std::size_t> visited;
	if (next[sourceIndex][destinationIndex] != INVALID_INDEX) {
		path.push_back(sourceIndex);
		visited.insert(sourceIndex);
		auto u = sourceIndex;
		while (u != destinationIndex) {
			u = next[u][destinationIndex];
			if (visited.count(u) > 0) {
				// OOPS, there is an endless loop over cycle
				std::cerr << "Warning: endless loop over cycle detected: ";
				const auto& pair = m_exchangeCurrencyPairsByIndex[u];
				std::cerr << "starting with: " << u << " (" << pair.first << '/' << pair.second
					<< "), path: ";
				bool first = true;
				for (const auto index : path) {
					const auto& pair = m_exchangeCurrencyPairsByIndex[index];
					if (first) {
						first = false;
					} else {
						std::cerr << " -> ";
					}
					std::cerr << index << " (" << pair.first << '/' << pair.second
						<< ')';
				}
				std::cerr << std::endl;
				path.clear();
				break;
			}
			path.push_back(u);
			visited.insert(u);
		}
	}

	return path;
}

void ExchangeRateProcessor::printPath(const std::string& sourceExchange,
	const std::string& sourceCurrency, const std::string& destinationExchange,
	const std::string& destinationCurrency, const std::vector<std::size_t>* path) const
{
	std::cout << BEST_RATES_BEGIN << ' ' << sourceExchange << ' ' <<  sourceCurrency
		<< ' ' << destinationExchange << ' ' << destinationCurrency << std::endl;
	if (path) {
		for (const auto index: *path) {
			const auto& pair = m_exchangeCurrencyPairsByIndex[index];
			std::cout << pair.first << ", " << pair.second << std::endl; 
		}
	}
	std::cout << BEST_RATES_END << std::endl;
}

#ifdef _DEBUG
void ExchangeRateProcessor::printAdjacencyTable() const
{
	std::cerr << "---------------------------------------------------" << std::endl;
	std::cerr << "Current adjacency table:" << std::endl;
	std::cerr << "---------------------------------------------------" << std::endl;
	for (std::size_t i = 0; i < m_adjacencyTable.size(); ++i) {
		const auto& pair = m_exchangeCurrencyPairsByIndex[i];
		std::cout << pair.first << '/' << pair.second << '\t';
		for (const auto& cell : m_adjacencyTable[i]) {
			std::cout << '\t' << cell.second;
		}
		std::cout << std::endl;
	}
	std::cerr << "---------------------------------------------------" << std::endl;
}


void ExchangeRateProcessor::printFloydWarsallTables(const std::vector<std::vector<double>>& rate,
	const std::vector<std::vector<std::size_t>>& next) const
{
	std::cerr << "---------------------------------------------------" << std::endl;
	std::cerr << "Current 'rate' table:" << std::endl;
	std::cerr << "---------------------------------------------------" << std::endl;
	for (std::size_t i = 0; i < rate.size(); ++i) {
		const auto& pair = m_exchangeCurrencyPairsByIndex[i];
		std::cout << pair.first << '/' << pair.second << '\t';
		for (const auto& cell : rate[i]) {
			std::cout << '\t' << cell;
		}
		std::cout << std::endl;
	}
	std::cerr << "---------------------------------------------------" << std::endl;
	std::cerr << "---------------------------------------------------" << std::endl;
	std::cerr << "Current 'next' table:" << std::endl;
	std::cerr << "---------------------------------------------------" << std::endl;
	for (std::size_t i = 0; i < next.size(); ++i) {
		const auto& pair = m_exchangeCurrencyPairsByIndex[i];
		std::cout << pair.first << '/' << pair.second << '\t';
		for (const auto& cell : next[i]) {
			std::cout << '\t' << cell;
		}
		std::cout << std::endl;
	}
	std::cerr << "---------------------------------------------------" << std::endl;
}
#endif
