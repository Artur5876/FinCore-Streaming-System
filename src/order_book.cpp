#include "order_book.hpp"
#include <iostream>
#include <iomanip>


OrderBook::OrderBook(const std::string& sym): symbol(sym) {};

void OrderBook::add_bid(double price, uint64_t quantity) {
    bid[price] += quantity;
}
void OrderBook::add_ask(double price, uint64_t quantity) {
    ask[price] += quantity;
}

double OrderBook::get_best_bid() const {
    return bid.empty() ? 0.0 : bid.begin()->first;
}
double OrderBook::get_best_ask() const {
    return ask.empty() ? 0.0 : ask.begin()->first;
}

double OrderBook::calculate_mid_price() const {
    if (bid.empty() || ask.empty()) return 0.0;
    return (get_best_bid() + get_best_ask()) / 2.0;
}

void OrderBook::update_from_tick(const Tick& tick) {
    double mid = calculate_mid_price();
    //For simulation: if price is within 1% of mid(for both sides)
    if (tick.side == Tick::Side::BID) {
        add_bid(tick.price, tick.volume);
        //also add a simulated ask (slightly higher price)
        add_ask(tick.price * 1.001, tick.volume * 0.8);
    } else {
        add_ask(tick.price, tick.volume);
        //simulated bid at slightly lower price
        add_bid(tick.price * 0.999, tick.volume * o.8);
    }
    
}

double OrderBook::get_spread() const  {
    return get_best_ask() - get_best_bid();
}

void OrderBook::print_summary() const {
    std::cout << "Symbol: " << symbol << "\n";
    std::cout << "- Best Bid: $" << std::fixed << std::setprecision(2) 
              << get_best_bid() << " (" << (bids.empty() ? 0 : bids.begin()->second) << " shares)\n";
    std::cout << "- Best Ask: $" << get_best_ask() << " (" << (asks.empty() ? 0 : asks.begin()->second) << " shares)\n";
    std::cout << "- Spread: $" << get_spread() << " (" 
              << std::setprecision(2) << (get_spread() / get_best_bid() * 100) << "%)\n";
}

void OrderBook::print_depth(int levels) const {
    std::cout << "\n=== " << symbol << " ORDER BOOK ===\n";
    
    std::cout << "BIDS: ";
    int count = 0;
    for (const auto& [price, quantity] : bids) {
        if (++count >= levels) break;
        std::cout << "$" << price << "(" << quantity << ")";
    }
    std::cout << "\nASKS: ";
    count =0;
    for (const auto& [price, quantity]: asks) {
        if (++count >= levels) break;
        std::cout << "$" << price << "(" << quantity << ")";
    }
    std::cout << "\n";
}

uint64_t OrderBook::getTotalBidVolume() const {
    uint64_t total = 0;

    //sum ALLL bid quantities
    for (const auto& [_, quantity] : bids) total += quantity;

    return total;
}

uint64_t OrderBook::getTotalAskVolume() const {
    uint64_t total = 0;

    //sum ALL asks quantities;
    for (const auto& [_, quantities] : asks) total += quantity;
    return total;
}
