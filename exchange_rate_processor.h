#ifndef EXCHANGE_RATE_PROCESSOR_H__
#define EXCHANGE_RATE_PROCESSOR_H__

#include "string_tokenizer.h"
#include "pair_hash.h"
#include <ctime>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class ExchangeRateProcessor {
public:
	// Constructor
	ExchangeRateProcessor();

	// Process incoming data.
	void processData(const std::string& data);

private:
	// Process rate info.
	void processInfo(StringTokenizer& data);

	// Process rate request
	void processRequest(StringTokenizer& data);

	// Parse single string field
	std::string parseStringField(StringTokenizer& data, const char* fieldName);

	// Parse single time field
	std::time_t parseTimestampField(StringTokenizer& data, const char* fieldName);

	// Parse single floating-point number field
	double parseDoubleField(StringTokenizer& data, const char* fieldName);

	// Adds new exchange/currency pair. Returns index of the new pair.
	std::size_t addExchangeCurrencyPair(const std::string& exchange, const std::string& currency);

	// Find existing exchange/currency pair. Returns index of pair or max std::sizet value of not found.
	std::size_t findExchangeCurrencyPair(const std::string& exchange,
		const std::string& currency) const;

	// Extends adjancency table by 1 row and 1 column
	void extendAdjancencyTable();

	// Provision brand new currency for all exchanges
	void provisionNewCurrency(const std::string& currency);

	// Provision new currency for given exchange
	void provisionCurrencyForExchange(const std::string& currency, const std::string& exchange);

	// Generate best exchange path from source to destination.
	std::vector<std::size_t> generateExchangePath(const std::size_t sourceIndex,
		const std::size_t destinationIndex) const;

	// Prints output headers and given path.
	// "path" can be nullptr, then just headers printed out.
	void printPath(const std::string& sourceExchange, const std::string&sourceCurrency,
		const std::string& destinationExchange, const std::string& destinationCurrency,
		const std::vector<std::size_t>* path) const;

#ifdef _DEBUG
	// Print adjacency table to stderr for debugging purposes
	void printAdjacencyTable() const;

	// Print Floyd-Warsall algorithm tables to stderr for debugging purposes
	void printFloydWarsallTables(const std::vector<std::vector<double>>& rate, 
		const std::vector<std::vector<std::size_t>>& next) const;
#endif

	//////////////////////// DATA /////////////////////////////////////////

	// All known exchanges
	std::unordered_set<std::string> m_exchanges;

	// All known currencies
	std::unordered_set<std::string> m_currencies;

	// Mapping of exchange/currency pair to its index
	std::unordered_map<std::pair<std::string, std::string>, std::size_t, pair_hash> 
		m_exchangeCurrencyPairToIndexMapping;

	// Stores exchange/currency pair names at respective index
	std::vector<std::pair<std::string, std::string>> m_exchangeCurrencyPairsByIndex;

	// Adjacency table of exchange rate graph
	// Value of table cell is pair, where first is rate timestamp and second is rate value
	std::vector<std::vector<std::pair<std::time_t, double>>> m_adjacencyTable;
};

#endif // EXCHANGE_RATE_PROCESSOR_H__
