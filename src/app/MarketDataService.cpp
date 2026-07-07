#include "/home/artur/Desktop/Financial-Core-Streaming-System/include/app/MarketDataService.hpp"

#include <exception>
#include <algorithm>

namespace fincore {
    MarketDataService::MarketDataService(
        IMarketDataSource& source,
        IQuoteCache& cache,
        std::vector<std::string> configured_symbols)
        :   source_(source),
            cache_(cache),
            configured_symbols_(std::move(configured_symbols)) {

        for(auto& symbol: configured_symbols_) {
            symbol = normalizeSymbol(symbol);
        }

        std::sort(configured_symbols_.begin(), configured_symbols_.end()) ;
        configured_symbols_.erase(
            std::unique(configured_symbols_.begin(), configured_symbols_.end()), configured_symbols_.end());
    }


    //Method that will look for anything in cache and return the data
    FetchResult MarketDataService::fetchAndCache(std::string_view raw_symbol) {
        const auto started = std::chrono::steady_clock::now();
        const std::string symbol = normalizeSymbol(raw_symbol);

        if (!isConfigured(symbol)) {
            ++stats_.failures;
            return {false, "symbol is not configured: " + symbol, std::nullopt, {}};
        }

        try {
            ++stats_.api_requests;
            auto quotes = source_.fetchQuotes({symbol});
            stats_.quotes_received += quotes.size();

            const auto it = std::find_if(
                quotes.begin(), quotes.end(),
                [&symbol] (const Quote& quote) {
                    return normalizeSymbol(quote.symbol) == symbol;
                });
            if (it == quotes.end()) {
                ++stats_.failures;
                const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started);
                stats_.last_operation = elapsed;
                return {false, "API returned no quote for " + symbol, std::nullopt, elapsed};
            }

            cache_.store(*it);
            ++stats_.cache_writes;

            const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - started);
            stats_.last_operation = elapsed;

            return {true, "quote fetched and cached", *it, elapsed};
        } catch (const std::exception& ex) {
            ++stats_.failures;
            const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - started);
            stats_.last_operation = elapsed;
            return {false, ex.what(), std::nullopt, elapsed};
        }
    }

    //Simplified function to fetch data and cache in Redis
    //It a combination of functions that had been implemented (so its like a container)
    BatchFetchResult MarketDataService::fetchAllAndCache() {
        const auto started = std::chrono::steady_clock::now();

        try {
            ++stats_.api_requests;
            auto quotes = source_.fetchQuotes(configured_symbols_);
            stats_.quotes_received += quotes.size();

            cache_.storeBatch(quotes);
            stats_.cache_writes += quotes.size();

            const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - started);
            stats_.last_operation = elapsed;

            return {
                    true,
                    "batch fetched and cached",
                    std::move(quotes),
                    elapsed
            };
        } catch (const std::exception& ex) {
            ++stats_.failures;
            const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - started);
            stats_.last_operation = elapsed;
            return {false, ex.what(), {}, elapsed};
        }


    }



    bool MarketDataService::isConfigured(std::string_view raw_symbol) const {
        const std::string symbol = normalizeSymbol(raw_symbol);

        return std::binary_search(
            configured_symbols_.begin(), configured_symbols_.end(), symbol);
    }








    const std::vector<std::string>& MarketDataService::symbols() const noexcept {
        return configured_symbols_;
    }


    const ServiceStats& MarketDataService::stats() const noexcept {
        return stats_;
    }

    std::string MarketDataService::normalizeSymbol(std::string_view symbol) {
        std::string normalized(symbol);
        std::transform(
            normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
                return static_cast<char>(std::toupper(ch));
            }
        );
        return normalized;
    }
}
