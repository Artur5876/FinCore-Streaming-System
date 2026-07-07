#pragma once
#include "/home/artur/Desktop/Financial-Core-Streaming-System/include/domain/Quote.hpp"

#include <string>
#include <vector>

namespace fincore {

    class IMarketDataSource {
        public:
            virtual ~IMarketDataSource() = default;

            //fetchQuote func that can be called internally without API knowing
            virtual std::vector<Quote> fetchQuotes(
                const std::vector<std::string>& symbols) = 0;
    };
}

