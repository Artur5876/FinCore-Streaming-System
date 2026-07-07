#pragma once
#include "/home/artur/Desktop/Financial-Core-Streaming-System/include/domain/Quote.hpp"
#include "/home/artur/Desktop/Financial-Core-Streaming-System/include/ports/IMarketDataSource.hpp"
#include "/home/artur/Desktop/Financial-Core-Streaming-System/include/ports/IQuoteCache.hpp"

#include <chrono>
#include <string>
#include <optional>
#include <vector>
#include <string_view>

namespace fincore {
    struct Quote;

    struct FetchResult {
        bool ok{false};
        std::string message;
        std::optional<Quote> quote;
        std::chrono::microseconds elapsed{};
    };

    struct BatchFetchResult {
        bool ok{false};
        std::string message;
        std::vector<Quote> quote;
        std::chrono::microseconds elapsed{};
    };

    struct ServiceStats {
        std::size_t api_requests{};
        std::size_t quotes_received{};
        std::size_t cache_writes{};
        std::size_t failures{};
        std::chrono::microseconds last_operation{};
    };

    class MarketDataService {
        public:
            MarketDataService(IMarketDataSource& source,
                    IQuoteCache& cache,
                    std::vector<std::string> configured_symbols);
            FetchResult fetchAndCache(std::string_view symbol);
            BatchFetchResult fetchAllAndCache();

            [[nodiscard]] std::optional<Quote> latest(std::string_view symbol) const;
            [[nodiscard]] bool cacheAvailable() const noexcept;
            [[nodiscard]] bool isConfigured(std::string_view symbol) const;
            [[nodiscard]] const std::vector<std::string>& symbols() const noexcept;
            [[nodiscard]] const ServiceStats& stats() const noexcept;
        private:
            static std::string normalizeSymbol(std::string_view symbol);

            IMarketDataSource& source_;
            IQuoteCache& cache_;
            std::vector<std::string> configured_symbols_;
            ServiceStats stats_;
    };
}

