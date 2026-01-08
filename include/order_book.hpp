#pragma once
#include <map>
#include <string>
#include <vector>
#include "tick.hpp"


class OrderBook {

private:
    std::string symbol;
    std::map<double, uint64_t, std::greater<double>> bids;//descending order
    std::map<double, uint64_t> asks;
    double calculate_mid_price() const;
public:
    explicit OrderBook(const std::string& symbol);

    // Core functionality
    void add_bid(double price, uint64_t quantity);
    void add_ask(double price, uint64_t quantity);
    void update_from_tick(const Tick& tick);
    
    // Getters
    double get_best_bid() const;
    double get_best_ask() const;
    double get_spread() const;
    
    // Display
    void print_summary() const;
    void print_depth(int levels = 5) const;
    
    // Stats
    size_t get_bid_levels() const { return bids.size(); }
    size_t get_ask_levels() const { return asks.size(); }
    uint64_t get_total_bid_volume() const;
    uint64_t get_total_ask_volume() const;
};
