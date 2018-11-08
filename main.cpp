#include "exchange_rate_processor.h"
#include <iostream>
#include <fstream>
#include <memory>

int main(int argc, char** argv)
{
	// Set input stream to standard input
	std::istream* inputStream = &std::cin;

	// In debug build we allow passing input file name on the command line,
	// it makes it easier to debug. In release build this functionality
	// is not included. 
#ifdef _DEBUG
	std::unique_ptr<std::ifstream> inputFile;
	if (argc > 1) {
		// Try open specified input file.
		inputFile.reset(new std::ifstream(argv[1]));
		if (!inputFile->is_open()) {
			// Failed to open file, report error and exit.
			std::cerr << "Error: Can't open input file " << argv[1] << std::endl;
			return 1;
		} else {
			// Set input stream to file stream
			inputStream = inputFile.get();
		}
	}
#endif

	// Create exchange processor instance
	ExchangeRateProcessor processor;

	// Read input stream line by line until end of stream or error,
	// process each line using processor.
	std::string line;
	while (std::getline(*inputStream, line)) {
		processor.processData(line);
	}

	// If EOF reached, return success exit code (0), otherwise indicate error
	return inputStream->eof() ? 0 : 2;
}
