#pragma once
#include "/home/artur/Desktop/Financial-Core-Streaming-System/include/domain/Quote.hpp"

#include <string_view>
#include <optional>
#include <vector>

namespace fincore {
class IQuoteCache{
    public:
        virtual ~IQuoteCache() = default;
        virtual void store(const Quote& quote) = 0;

        //The default keeps adapters simple. A Redis adapter can override this
        //later and use it without changing MarketDataService or the CLI
        virtual void storeBatch(const std::vector<Quote>& quotes) {
            for (const auto& quote : quotes) {
                store(quote);
            }
        }

        virtual std::optional<Quote> loadLatest(std::String_view symbol) const = 0;
        virtual bool ping() const noexcept = 0;
};
}
