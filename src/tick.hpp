#pragma once
#include <string>

struct Tick {
    std::string symbol;
    double price;
    size_t volume;
    long timestamp;
};
