#include "api/alpha_vantage_client.hpp"
#include "storage/redis_client.hpp"
#include "core/order_book.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <atomic>

using namespace fincore;

std::atomic<bool> running{true};

void signal_handler(int) {
    running = false;
}

int main() {
    signal(SIGINT, signal_handler);

    std::cout << "=== FinCore Real-time Market Data ===\n";

    const std::string api_key = "BKC60G7LB0JYAVXN";
    std::vector<std::string> symbols = {"AAPL", "GOOGL", "MSFT"};

    try {
        AlphaVantageClient av_client(api_key);
        RedisClient redis_client("127.0.0.1", 6379);

        // Create order books for each symbol
        std::map<std::string, OrderBook> order_books;
        for (const auto& symbol : symbols) {
            order_books.emplace(symbol, OrderBook(symbol));
        }

        std::cout << "Starting real-time data feed. Press Ctrl+C to stop.\n\n";

        int iteration = 0;
        while (running) {
            std::cout << "\n--- Update " << ++iteration << " ---\n";

            for (const auto& symbol : symbols) {
                auto quote = av_client.get_global_quote(symbol);

                if (quote.price > 0) {
                    // sktore in Redis
                    redis_client.store_quote(symbol, quote);

                    Tick tick(symbol, quote.price, quote.volume, Side::BID);

                    //store tick in Redis Stream
                    redis_client.store_tick(tick);

                    // Update order book
                    auto& book = order_books.at(symbol);
                    book.update_from_tick(tick);

                    //store in Redis
                    redis_client.update_order_book(
                        symbol,
                        book.get_top_bids(5),
                        book.get_top_asks(5)
                    );

                    //summary
                    std::cout << symbol << ": $" << quote.price
                              << " (" << quote.change_percent << "%)\n";
                }

                //13 seconds for Alpha Vantage
                if (running) std::this_thread::sleep_for(std::chrono::seconds(13));
            }

            //summary every 3 iterations
            if (iteration % 3 == 0) {
                std::cout << "\n=== Order Book Snapshots ===\n";
                for (const auto& [symbol, book] : order_books) {
                    book.print_summary();
                    std::cout << "\n";
                }
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
