#include "/home/artur/Desktop/Financial-Core-Streaming-System/include/api/alpha_vantage_client.hpp"
#include "/home/artur/Desktop/Financial-Core-Streaming-System/include/storage/redis_client.hpp"
#include "/home/artur/Desktop/Financial-Core-Streaming-System/include/core/order_book.hpp"
#include "/home/artur/Desktop/Financial-Core-Streaming-System/include/core/types.hpp"

#include <cstdlib>
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <string>
#include <unordered_map>
#include <map>
#include <stdexcept>
using namespace fincore;

std::atomic<bool> running{true};

//Configuration helper with environment setup
static std::string env_or(const char* name, std::string fallback) {
    const char* val = std::getenv(name);
    return val ? std::string(val) : std::move(fallback);
}


//Print a Quote
static void print_quote(const fincore::Quote& q) {
    std::cout
        << "\n--- Quote: " << q.symbol << " ---------------------\n"
        << "  Price  : $" << q.price      << "\n"
        << "  Open   : $" << q.open       << "\n"
        << "  High   : $" << q.high       << "\n"
        << "  Low    : $" << q.low        << "\n"
        << "  Volume :  " << q.volume     << "\n"
        << "  Chg %  :  " << q.change_pct << "%\n"
        << "  Source :  " << q.source     << "\n";
}

//Print an OrderBookSnapshot
static void print_snapshot(const fincore::OrderBookSnapshot& snap) {
    std::cout
        << "\n--- OrderBook Snapshot: " << snap.symbol << " -----------------\n"
        << "  Best Bid : $" << snap.best_bid      << "\n"
        << "  Best Ask : $" << snap.best_ask      << "\n"
        << "  Mid      : $" << snap.mid_price     << "\n"
        << "  Spread   : $" << snap.spread        << "\n"
        << "  Imbalance:  " << snap.imbalance     << "\n"
        << "  Bid Vol  :  " << snap.total_bid_vol << "\n"
        << "  Ask Vol  :  " << snap.total_ask_vol << "\n";
}

// //Data population from L2 storage
// Here I simulate a 5-level book centred on the quote price.
static void populate_book_from_quote(fincore::OrderBook& book,
                                     const fincore::Quote& q)
{
    book.clear();

    const double tick = 0.01;  //$0.01 per level

    // 5 bid levels below best bid, decreasing volume
    for (int i = 1; i <= 5; ++i) {
        fincore::Price  price  = q.price - (i * tick);
        fincore::Volume volume = static_cast<fincore::Volume>(q.volume / 10 / i);
        if (price > 0.0 && volume > 0)
            book.set_bid(price, volume);
    }

    // 5 ask levels above, increasing volume
    for (int i = 1; i <= 5; ++i) {
        fincore::Price  price  = q.price + (i * tick);
        fincore::Volume volume = static_cast<fincore::Volume>(q.volume / 10 / i);
        if (price > 0.0 && volume > 0)
            book.set_ask(price, volume);
    }
}



int main() {

    //Reading the environment variables without enclosing the information
    //in the files
    const std::string av_api_key   = env_or("AV_API_KEY",    "demo");
    const std::string redis_host   = env_or("REDIS_HOST",    "127.0.0.1");
    const int         redis_port   = std::stoi(env_or("REDIS_PORT", "6379"));
    const int         poll_seconds = std::stoi(env_or("POLL_SECONDS", "60"));

    //Symbols data of which is going to be checked via API request
    const std::vector<std::string> symbols = {"AAPL", "MSFT", "GOOGL"};

    std::cout << "[FinCore] Starting Financial Core Streaming System\n"
              << "  Redis  : " << redis_host << ":" << redis_port << "\n"
              << "  AV key : " << av_api_key.substr(0, 4) << "****\n"
              << "  Poll   : every " << poll_seconds << "s\n\n";


    //API client initialization
    fincore::AlphaVantageClient av_client(av_api_key,
                                          std::chrono::seconds{poll_seconds});

    std::unique_ptr<fincore::RedisClient> redis;
    try {
        redis = std::make_unique<fincore::RedisClient>(redis_host, redis_port);
    } catch (const std::exception& e) {
        std::cerr << "[FinCore] Cannot connect to Redis: " << e.what() << "\n";
        return 1;
    }

    //OrderBook will be shown as only one per symbol
    std::unordered_map<std::string, fincore::OrderBook> books;
    for (const auto& sym : symbols)
        books.emplace(sym, fincore::OrderBook(sym));

    //Main poll loop
    while (true) {
        if (!redis->is_connected()) {
            std::cerr << "[FinCore] Redis disconnected --> retrying next cycle\n";
        }

        for (const auto& symbol : symbols) {
            std::cout << "\n[FinCore] Fetching quote for " << symbol << " ...\n";

            auto maybe_quote = av_client.get_quote(symbol);
            if (!maybe_quote) {
                std::cerr << "[FinCore] No quote for " << symbol << " — skipping\n";
                continue;
            }

            const fincore::Quote& quote = *maybe_quote;

            if (av_client.last_was_cached())
                std::cout << "  (served from cache)\n";

            print_quote(quote);

            // Store quote in Redis
            if (!redis->store_quote(symbol, quote))
                std::cerr << "[FinCore] Failed to store quote for " << symbol << "\n";

            // Update order book and push snapshot
            auto& book = books.at(symbol);
            populate_book_from_quote(book, quote);

            //build bid/ask maps for Redis (order_book.hpp exposes them read-only)
            std::map<fincore::Price, fincore::Volume> bids_map(book.bids().begin(), book.bids().end());
            std::map<fincore::Price, fincore::Volume> asks_map(book.asks().begin(), book.asks().end());
            redis->update_order_book(symbol, bids_map, asks_map);

            const auto snap = book.snapshot();
            print_snapshot(snap);
        }

        std::cout << "\n[FinCore] Sleeping for " << poll_seconds << "s ...\n";
        std::this_thread::sleep_for(std::chrono::seconds{poll_seconds});
    }

    return 0;
}
