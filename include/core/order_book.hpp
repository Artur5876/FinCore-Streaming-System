#pragma once
//Bids stored descending (best bid = highest price = begin())
//Asks stored ascending  (best ask = lowest price = begin())


#include "types.hpp"
#include <map>
#include <stdexcept>

namespace fincore {
    class OrderBook {
        private:
            Symbol symbol_;

            //bids with highest price first;
            std::map<Price, Volume, std::greater<Price>> bids_;
            //Asks with lowest price first;
            std::map<Price, Volume> asks_;

            void remove_zero_levels(auto& map);
        public:
            explicit OrderBook(Symbol symbol) : symbol_(std::move(symbol)) {
                if (symbol_.empty())
                    throw std::invalid_argument("OrderBook: symbol must not be empty!!");
            }

        [[nodiscard]] const Symbol& symbol() const noexcept { return symbol_; }

        //The number of distinct price levels on each side
        [[nodiscard]] std::size_t bid_depth() const noexcept { return bids_.size(); }
        [[nodiscard]] std::size_t ask_depth() const noexcept { return asks_.size(); }
        [[nodiscard]] bool        empty()     const noexcept { return bids_.empty() && asks_.empty(); }

        //MUTATIONS
        //
        //Set or update a price level, if volume == 0 than level will be removed
        void set_bid(Price price, Volume volume);
        void set_ask(Price price, Volume volume);

        //Replace the entire side entirely (for convenience)
        void replace_bids(const std::map<Price, Volume>& levels);
        void replace_asks(const std::map<Price, Volume>& levels);

        //Clear both sides
        void clear() noexcept;

        //Best prices selection
        [[nodiscard]] std::optional<Price>  best_bid()    const noexcept;
        [[nodiscard]] std::optional<Price>  best_ask()    const noexcept;
        [[nodiscard]] std::optional<double> mid_price()   const noexcept;
        [[nodiscard]] std::optional<double> spread()      const noexcept;

        //Aggregaates
        [[nodiscard]] Volume total_bid_volume() const noexcept;
        [[nodiscard]] Volume total_ask_volume() const noexcept;


        //(bid_vol - ask_vol) / (bid_vol + ask_vol)  is [-1, 1]
        //Returns 0.0 when both sides are empty.
        [[nodiscard]] double imbalance() const noexcept;


        //Snapshot
        //an OrderBookSnapshot that is suitable for persisting to TimescaleDB / Redis
        [[nodiscard]] OrderBookSnapshot snapshot(TimePoint at = {}) const;

        //read-only access to the maps that are serialised in Redis
        [[nodiscard]] const std::map<Price, Volume, std::greater <Price>>& bids() const noexcept{ return bids_; }
        [[nodiscard]] const std::map<Price, Volume>& asks() const noexcept { return asks_; }
    };


}

