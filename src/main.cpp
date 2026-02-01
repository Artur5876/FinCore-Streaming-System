#include "/home/arturromanov/Documents/Financial-Core-Streaming-Project/src/api/alpha_vantage_client.hpp"
#include "/home/arturromanov/Documents/Financial-Core-Streaming-Project/include/redis_client.hpp"
#include <iostream>

int main() {
    std::cout << "=== Alpha Vantage to Redis Demo ===" << std::endl;

    //https://www.alphavantage.co/support/#api-key
    const std::string api_key = "BKC60G7LB0JYAVXN";

    // List of symbols to track
    std::vector<std::string> symbols = {"AAPL", "GOOGL", "MSFT"};
    //
    // std::string track_symbol;
    // std::cout << "Enter track symbol: ";
    // std::cin >> track_symbol;
    // std::cout << std::endl;
    // for (track)

    try {
        // Initialize clients
        AlphaVantageClient av_client(api_key);
        RedisClient redis_client("127.0.0.1", 6379);

        std::cout << "Clients initialized successfully!\n" << std::endl;

        // Fetch and store each symbol
        for (const auto& symbol : symbols) {
            std::cout << "Processing " << symbol << "..." << std::endl;

            // Get quote from Alpha Vantage
            auto quote = av_client.get_global_quote(symbol);

            if (quote.price > 0) {
                // Store in Redis
                redis_client.store_quote(symbol, quote);

                // Display the quote
                std::cout << "  Price: $" << quote.price << std::endl;
                std::cout << "  Change: " << quote.change_percent << "%" << std::endl;
                std::cout << "  Volume: " << quote.volume << std::endl;
                std::cout << "  Stored in Redis ✓\n" << std::endl;

                // Retrieve from Redis to verify
                auto cached = redis_client.get_quote(symbol);
                std::cout << "  Retrieved from Redis: $" << cached.price << "\n" << std::endl;

            } else {
                std::cout << "  Failed to fetch quote for " << symbol << std::endl;
            }

            // Respect Alpha Vantage rate limits (5 calls/minute free tier)
            // Wait 13 seconds between calls
            if (symbol != symbols.back()) {
                std::cout << "Waiting 13 seconds for rate limiting..." << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(13));
            }
        }

        // Example: Get all stored quotes from Redis
        std::cout << "\n=== All Stored Quotes from Redis ===" << std::endl;
        for (const auto& symbol : symbols) {
            auto quote = redis_client.get_quote(symbol);
            if (quote.price > 0) {
                std::cout << symbol << ": $" << quote.price
                          << " (" << quote.change_percent << "%)" << std::endl;
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "\n=== Program Completed ===" << std::endl;
    return 0;
}