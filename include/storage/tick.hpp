#pragma once

#include <string>
#include <chrono>
#include <map>

namespace fincore {

//strong types for better code clarity
using Price = double;
using Volume = uint64_t;
using Symbol = std::string;

enum class Side { BID, ASK };

struct Tick {
    Symbol symbol;
    Price price;
    Volume volume;
    Side side;
    std::chrono::system_clock::time_point timestamp;

    Tick(Symbol sym, Price p, Volume v, Side s)
        : symbol(std::move(sym))
        , price(p)
        , volume(v)
        , side(s)
        , timestamp(std::chrono::system_clock::now()) {}
};

//move from alpha_vantage_client
struct Quote {
    Symbol symbol;
    Price price;
    Price open;
    Price high;
    Price low;
    Volume volume;
    double change;
    double change_percent;
    std::string timestamp;
};

}
