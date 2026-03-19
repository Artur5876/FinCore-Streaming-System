#include "/home/arturromanov/untitled/Financial-Core-Streaming-System/include/core/order_book.hpp"
#include <iostream>
#include <iomanip>
#include <thread>

namespace fincore {
    OrderBook::OrderBook(const std::string& sym): symbol_(sym) {};

    void OrderBook::add_bid(double price, uint64_t quantity) {
        bids_[price] += quantity;
    }
    void OrderBook::add_ask(double price, uint64_t quantity) {
        asks_[price] += quantity;
    }

    double OrderBook::get_best_bid() const {
        return bids_.empty() ? 0.0 : bids_.begin()->first;
    }
    double OrderBook::get_best_ask() const {
        return asks_.empty() ? 0.0 : asks_.begin()->first;
    }

    std::map<Price, Volume> OrderBook::get_top_bids(int levels) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::map<Price, Volume> result;
        int count = 0;
        for (const auto& [price, vol] : bids_) {
            if (count++ >= levels) break;
            result[price] = vol;
        }
        return result;
    }

    std::map<Price, Volume> OrderBook::get_top_asks(int levels) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::map<Price, Volume> result;
        int count = 0;
        for (const auto& [price, vol] : asks_) {
            if (count++ >= levels) break;
            result[price] = vol;
        }
        return result;
    }

    double OrderBook::calculate_mid_price() const {
        if (bids_.empty() || asks_.empty()) return 0.0;
        return (get_best_bid() + get_best_ask()) / 2.0;
    }

    void OrderBook::update_from_tick(const Tick& tick) {
        double mid = calculate_mid_price();
        //For simulation: if price is within 1% of mid(for both sides)
        if (tick.side == fincore::Side::BID) {
            add_bid(tick.price, tick.volume);
            //also add a simulated ask (slightly higher price)
            add_ask(tick.price * 1.001, tick.volume * 0.8);
        } else {
            add_ask(tick.price, tick.volume);
            //simulated bid at slightly lower price
            add_bid(tick.price * 0.999, tick.volume * 0.8);
        }

    }

    double OrderBook::get_spread() const  {
        return get_best_ask() - get_best_bid();
    }

    void OrderBook::print_summary() const {
        std::cout << "Symbol: " << symbol_ << "\n";
        std::cout << "- Best Bid: $" << std::fixed << std::setprecision(2) <<
                   get_best_bid() << " (" << (bids_.empty() ? 0 : bids_.begin()->second) << " shares)\n";
        std::cout << "- Best Ask: $" << get_best_ask() << " (" << (asks_.empty() ? 0 : asks_.begin()->second) << " shares)\n";
        std::cout << "- Spread: $" << get_spread() << " (" <<
                   std::setprecision(2) << (get_spread() / get_best_bid() * 100) << "%)\n";
    }

    void OrderBook::print_depth(int levels) const {
        std::cout << "\n=== " << symbol_ << " ORDER BOOK ===\n";

        std::cout << "BIDS: ";
        int count = 0;
        for (const auto& [price, quantity] : bids_) {
            if (++count >= levels) break;
            std::cout << "$" << price << "(" << quantity << ")";
        }
        std::cout << "\nASKS: ";
        count =0;
        for (const auto& [price, quantity]: asks_) {
            if (++count >= levels) break;
            std::cout << "$" << price << "(" << quantity << ")";
        }
        std::cout << "\n";
    }

    uint64_t OrderBook::get_total_bid_volume() const {
        uint64_t total = 0;

        //sum ALLL bid quantities
        for (const auto& [_, quantity] : bids_) total += quantity;

        return total;
    }

    uint64_t OrderBook::get_total_ask_volume() const {
        uint64_t total = 0;

        //sum ALL asks quantities;
        for (const auto& [_, quantity] : asks_) total += quantity;
        return total;
    }
}