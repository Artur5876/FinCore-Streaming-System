#pragma once

// Fetches market quotes from the Alpha Vantage REST API.
#include "core/types.hpp"

#include <functional>
#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>

namespace fincore {

    class AlphaVantageClient {
        public:
            //fetch function holder for testing the API
            using FetchFunction = std::function<std::string(const Symbol&)>;
        //cache_ttl: is how long a quote is considered flesh before re-fetching
        //default is 60 s is safe for the 25 req/day limit
            explicit AlphaVantageClient(std::string api_key,
                                        std::chrono::seconds cache_ttl = std::chrono::seconds{60},
                                        FetchFunction fetch_function = {});

            ~AlphaVantageClient();

            //Fetch the latest Global Quote for symbol
            //Or returns nullopt on network error, API rate-limit, or parse failure
            [[nodiscard]] std::optional<Quote> get_quote(const Symbol& symbol);

            //Bypass the cache and always hit the network
            [[nodiscard]] std::optional<Quote> get_quote_fresh(const Symbol& symbol);

            //True if the last request hit the cache
            [[nodiscard]] bool last_was_cached() const noexcept { return last_was_cached_;}


        private:
            std::string             api_key_;
            std::chrono::seconds    cache_ttl_;
            bool                    last_was_cached_{false};
            FetchFunction           fetch_function_;

            struct CacheEntry {
                Quote quote;
                std::chrono::steady_clock::time_point fetched_at;
            };
            std::unordered_map<Symbol, CacheEntry> cache_;

            //We are going to perform real HTTP get request and return JSON content
            //Empty string will be return in case of failure to establish api-request
            std::string fetch_json(const Symbol& symbol);

            //Parse the global quote Json block into fincore::Quote
            std::optional<Quote> parse_quote(const std::string& json, const Symbol& symbol);

            //JSON field helper
            static bool extract_double(const std::string& json, const std::string& key, double& out);
            static bool extract_string(const std::string& json, const std::string& key, std::string& out);
            };
}
