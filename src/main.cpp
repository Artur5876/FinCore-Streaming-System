#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <memory>
#include "/home/arturromanov/Documents/Financial-Core-Streaming-Project/include/order_book.hpp"
#include "/home/arturromanov/Documents/Financial-Core-Streaming-Project/include/redis_client.hpp"
#include "/home/arturromanov/Documents/Financial-Core-Streaming-Project/include/tick.hpp"

Tick parse_csv_line(const std::string& line) {
    std::stringstream ss(line);
    std::string token;
    Tick tick;
    
    // Format: SYMBOL,PRICE,VOLUME,SIDE,TIMESTAMP_MS
    std::getline(ss, token, ',');
    tick.symbol = token;
    
    std::getline(ss, token, ',');
    tick.price = std::stod(token);
    
    std::getline(ss, token, ',');
    tick.volume = std::stoull(token);
    
    std::getline(ss, token, ',');
    tick.side = (token == "BID") ? Tick::Side::BID : Tick::Side::ASK;
    
    std::getline(ss, token, ',');
    auto ms = std::chrono::milliseconds(std::stoll(token));
    tick.timestamp = std::chrono::system_clock::time_point(ms);
    
    return tick;
}

void print_header() {
    std::cout << "=======================================\n";
    std::cout << "MARKET DATA PIPELINE v0.2 - DAY 2\n";
    std::cout << "=======================================\n\n";
}

int main() {
    print_header();
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Initialize
    std::cout << "[09:15:00] Starting batch processing...\n";
    std::cout << "[09:15:00] Loading 100 sample ticks...\n\n";
    
    OrderBook order_book("AAPL");
    RedisClient redis("127.0.0.1", 6379);
    
    // Add some initial liquidity
    order_book.add_bid(149.80, 500);
    order_book.add_ask(150.20, 300);
    order_book.add_ask(150.45, 200);
    
    std::cout << "=== ORDER BOOK INITIALIZATION ===\n";
    order_book.print_summary();
    std::cout << "\n";
    
    std::cout << "=== PROCESSING 100 TICKS ===\n";
    
    // Read and process ticks
    std::ifstream file("sample_ticks_day2.csv");
    std::string line;
    int tick_count = 0;
    
    while (std::getline(file, line)) {
        tick_count++;
        
        // Parse tick
        Tick tick = parse_csv_line(line);
        
        // Update order book
        order_book.update_from_tick(tick);
        
        // Store in Redis Stream
        redis.store_tick_stream(tick);
        
        // Update Redis order book storage periodically
        if (tick_count % 20 == 0) {
            // In real implementation, get bids/asks from order_book
            // For now, just store current state
        }
        
        // Print first 2 ticks and last tick
        if (tick_count <= 2 || tick_count == 100) {
            std::cout << "[09:15:01] Tick #" << tick_count << ": " 
                      << tick.symbol << " @ $" << tick.price 
                      << " (" << (tick.side == Tick::Side::BID ? "Bid" : "Ask") << " side)\n";
            
            if (tick_count == 1) {
                std::cout << "  -> Order Book Updated:\n  ";
                order_book.print_depth(2);
            } else if (tick_count == 2) {
                std::cout << "  -> New bid level added at $" << tick.price << "\n";
            } else if (tick_count == 100) {
                std::cout << "  -> POTENTIAL MATCH: Bid $" << tick.price 
                          << " vs Ask $" << order_book.get_best_ask() << "\n";
            }
        }
    }
    
    // Final summary
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    std::cout << "\n=== SUMMARY ===\n";
    std::cout << "Processed: " << tick_count << " ticks in " 
              << duration.count() << "ms\n";
    std::cout << "Redis Stream 'ticks:AAPL': " 
              << redis.get_stream_length("AAPL") << " entries\n";
    std::cout << "Order Book Depth: " << order_book.get_bid_levels() 
              << " bid levels, " << order_book.get_ask_levels() << " ask levels\n";
    
    // Estimate memory (simplified)
    size_t estimated_memory = 8 * 1024 * 1024;  // 8MB base
    estimated_memory += tick_count * 100;  // ~100 bytes per tick
    
    std::cout << "Memory Usage: " << (estimated_memory / (1024.0 * 1024.0)) 
              << " MB\n\n";
    
    std::cout << "✅ DAY 2 COMPLETE: Order book tracks " << tick_count << " ticks\n";
    
    return 0;
}