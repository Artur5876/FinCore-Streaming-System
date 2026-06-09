#pragma once
#include "/home/arturromanov/untitled/Financial-Core-Streaming-System/include/api/alpha_vantage_client.hpp"
#include <hiredis/hiredis.h>
#include <string>
#include <iostream>
#include <cstdlib>
#include "/home/arturromanov/untitled/Financial-Core-Streaming-System/include/storage/tick.hpp"
#include <map>
#include <sw/redis++/redis++.h>
namespace fincore {

class RedisClient {
public:
    //constructor with connection pooling
    explicit RedisClient(const std::string& host = "127.0.0.1", int port = 6379);

    //modern: Returns bool for success/failure
    bool store_quote(const Symbol& symbol, const Quote& quote);

    //rturns optional (C++17)
    std::optional<Quote> get_quote(const Symbol& symbol);

    bool store_tick(const Tick& tick);


    void update_order_book(const Symbol& symbol,
                          const std::map<Price, Volume>& bids,
                          const std::map<Price, Volume>& asks);

    //connection health
    bool is_connected() const;

private:
    std::unique_ptr<sw::redis::Redis> redis_;
    std::string connection_string_;

    // Helper to create Redis key names consistently
    std::string quote_key(const Symbol& symbol) const;
    std::string quote_history_key(const Symbol& symbol) const;
    std::string tick_stream_key(const Symbol& symbol) const;
};

}
