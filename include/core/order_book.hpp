#pragma once
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include "/home/arturromanov/untitled/Financial-Core-Streaming-System/include/storage/tick.hpp"

namespace fincore {
class OrderBook {

private:
    std::string symbol_;
    std::map<Price, Volume, std::greater<double>> bids_;//descending order
    std::map<Price, Volume> asks_;
    mutable std::mutex mutex_;
    double calculate_mid_price() const;
public:
    explicit OrderBook(const std::string& symbol);

    // Core functionality
    void add_bid(double price, uint64_t quantity);
    void add_ask(double price, uint64_t quantity);

    std::map<fincore::Price, fincore::Volume> get_top_bids(int levels) const;
    std::map<fincore::Price, fincore::Volume> get_top_asks(int levels) const;


    void update_from_tick(const fincore::Tick& tick);
    
    // Getters
    double get_best_bid() const;
    double get_best_ask() const;
    double get_spread() const;
    
    // Display
    void print_summary() const;
    void print_depth(int levels = 5) const;
    
    // Stats
    size_t get_bid_levels() const { return bids_.size(); }
    size_t get_ask_levels() const { return asks_.size(); }
    uint64_t get_total_bid_volume() const;
    uint64_t get_total_ask_volume() const;
};
}