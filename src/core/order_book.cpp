#include "core/order_book.hpp"
#include <stdexcept>

namespace fincore {
    void OrderBook::remove_zero_levels(auto& map) {
        for (auto it = map.begin(); it != map.end(); ) {
            it = (it->second == 0) ? map.erase(it) : std::next(it);
        }
    }

    //Mutations
    void OrderBook::set_bid(Price price, Volume volume) {
        if(price <= 0.0)
            throw std::invalid_argument("OrderBook::set_bid: price must be > 0");
        if(volume == 0) {
            bids_.erase(price);
        } else {
            bids_[price] = volume;
        }
    }

    void OrderBook::set_ask(Price price, Volume volume) {
        if (price <= 0.0)
            throw std::invalid_argument("OrderBook::set_ask: price must be > 0");
        if (volume == 0)
            asks_.erase(price);
        else
            asks_[price] = volume;
    }

    void OrderBook::replace_bids(const std::map<Price, Volume>& levels) {
        bids_.clear();
        for (const auto& [price, vol] : levels)
            set_bid(price, vol);
    }

    void OrderBook::replace_asks(const std::map<Price, Volume>& levels) {
        asks_.clear();
        for (const auto& [price, vol] : levels)
            set_ask(price, vol);
    }

    void OrderBook::clear() noexcept {
        bids_.clear();
        asks_.clear();
    }

    //Best Prices
    std::optional<Price> OrderBook::best_bid() const noexcept {
        if(bids_.empty()) return std::nullopt;
        return bids_.begin()->first; //highest price first
    }

    std::optional<Price> OrderBook::best_ask() const noexcept {
        if(asks_.empty()) return std::nullopt;
        return asks_.begin()->first; //lowest
    }


    std::optional<double> OrderBook::mid_price() const noexcept {
        auto bid = best_bid();
        auto ask = best_ask();
        if (!bid || !ask) return std::nullopt;
        return (*bid + *ask) * 0.5;
    }

    std::optional<double> OrderBook::spread() const noexcept {
        auto bid = best_bid();
        auto ask = best_ask();
        if (!bid || !ask) return std::nullopt;
        return *ask - *bid;
    }

    //Aggregates
    Volume OrderBook::total_bid_volume() const noexcept {
        Volume total = 0;
        for (const auto& [_, vol] : bids_) total += vol;
        return total;
    }

    Volume OrderBook::total_ask_volume() const noexcept {
        Volume total = 0;
        for (const auto& [_, vol] : asks_) total += vol;
        return total;
    }

    double OrderBook::imbalance() const noexcept {
        const double bid_v = static_cast<double>(total_bid_volume());
        const double ask_v = static_cast<double>(total_ask_volume());
        const double total = bid_v + ask_v;
        return (total == 0.0) ? 0.0 : (bid_v - ask_v) / total;
    }

    //Snapshot export
    OrderBookSnapshot OrderBook::snapshot(TimePoint at) const {
        OrderBookSnapshot snap;
        snap.symbol        = symbol_;
        snap.best_bid      = best_bid().value_or(0.0);
        snap.best_ask      = best_ask().value_or(0.0);
        snap.mid_price     = mid_price().value_or(0.0);
        snap.spread        = this->spread().value_or(0.0);
        snap.total_bid_vol = total_bid_volume();
        snap.total_ask_vol = total_ask_volume();
        snap.imbalance     = imbalance();
        snap.snapshot_time = (at == TimePoint{})
                                ? std::chrono::time_point_cast<std::chrono::microseconds>(
                                      std::chrono::system_clock::now())
                                : at;
        return snap;
    }
}
