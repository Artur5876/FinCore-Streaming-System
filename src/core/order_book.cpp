#include "/home/arturromanov/Documents/Financial-Core-Streaming-Project/include/core/order_book.hpp"
#include <iostream>
#include <iomanip>
#include <numeric>

namespace fincore {

OrderBook::OrderBook(Symbol symbol): symbol_(std::move(symbol)) {};

void OrderBook::add_bid(Price price, Volume quantity) {
    std::lock_guard<std::mutex> lock(mutex_);
    bids_[price] += quantity;
}

void OrderBook::add_ask(Price price, Volume quantity) {
    std::lock_guard<std::mutex> lock(mutex_);
    asks_[price] += quantity;
}

Price OrderBook::get_best_bid() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return bids_.empty() ? 0.0 : bids_.begin()->first;
}
Price OrderBook::get_best_ask() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return asks_.empty() ? 0.0 : asks_.begin()->first;
}

Price OrderBook::get_mid_price() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (bids_.empty() || asks_.empty()) return 0.0;
    return (bids_.begin()->first + asks_.begin()->first) / 2.0;
}

Price OrderBook::get_spread() const {
    return get_best_ask() - get_best_bid();
}

double OrderBook::get_spread_percent() const {
    Price bid = get_best_bid();
    if (bid <= 0) return 0.0;
    return (get_spread() / bid) * 100.00;
}

void OrderBook::update_from_tick(const Tick& tick) {
    if (tick.side == Side::BID) {
        add_bid(tick.price, tick.volume);
        add_ask(tick.price * 1.001, tick.volume * 0.8);
    } else {
        add_ask(tick.price, tick.volume);
        add_bid(tick.price * 0.999, tick.volume * 0.8);
    }
}

double OrderBook::get_imbalance() const {
    std::lock_guard<std::mutex> lock(mutex_);
    Volume bid_volume = 0, ask_volume = 0;

    //sum top 5 levels
    int level = 0;
    for (const auto& [_, vol]: asks_) {
        if (++level >= 5) break;
        ask_volume += vol;
    }

    level =0;
    for (const auto& [_, vol]: bids_) {
        if (++level >= 5) break;
        bid_volume += vol;
    }

    Volume total = ask_volume + bid_volume;
    if (total == 0) return 0.0;

    return static_cast<double>(bid_volume - ask_volume) / total;
}

std::map<Price, Volume> OrderBook::get_top_bids(int level) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::map<Price, Volume> result;
    int count = 0;

    for (const auto& [price, volume] : bids_) {
        if (count++ >= level) break;
        result[price] = volume;
    }
    return result;
}

std::map<Price, Volume> OrderBook::get_top_asks(int level) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::map<Price, Volume> result;
    int count = 0;
    for (const auto& [price, volume] : asks_) {
        if (count++ >= level) break;
        result[price] = volume;
    }
    return result;
}


void OrderBook::print_summary() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "Symbol: " << symbol_ << "\n";

    Price bid = bids_.empty() ? 0.0 : bids_.begin()->first;
    Price ask = asks_.empty() ? 0.0 : asks_.begin()->first;
    Volume bid_vol = bids_.empty() ? 0 : bids_.begin()->second;
    Volume ask_vol = asks_.empty() ? 0 : asks_.begin()->second;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "- Best Bid: $" << bid << " (" << bid_vol << ")\n";
    std::cout << "- Best Ask: $" << ask << " (" << ask_vol << ")\n";
    std::cout << "- Spread: $" << get_spread() << " ("
              << std::setprecision(2) << get_spread_percent() << "%)\n";
    std::cout << "- Imbalance: " << std::setprecision(2) << get_imbalance() << "\n";
}

void OrderBook::print_depth(int levels) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "\n=== " << symbol_ << " ORDER BOOK ===\n";

    std::cout << "BIDS: ";
    int count = 0;

    for (const auto& [price, quantity]: bids_) {
        if (count++ >= levels) break;
        std::cout << "$" << price << "(" << quantity << ") ";
    }

    std::cout << "\nASKS: ";
    count = 0;
    for (const auto& [price, quantity] : asks_) {
        if (++count > levels) break;
        std::cout << "$" << price << "(" << quantity << ") ";
    }
    std::cout << "\n";
}


uint64_t OrderBook::get_total_bid_volume() const {
    std::lock_guard<std::mutex> lock(mutex_);
    Volume total = 0;
    //sum ALLL bid quantities
    for (const auto& [_, quantity] : bids_) total += quantity;

    return total;
}

uint64_t OrderBook::get_total_ask_volume() const {
    std::lock_guard<std::mutex> lock(mutex_);
    Volume total = 0;
    //sum ALL asks quantities;
    for (const auto& [_, quantity] : asks_) total += quantity;
    return total;
}
}
