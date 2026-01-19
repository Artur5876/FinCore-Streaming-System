#pragma once
#include <string>
#include <chrono>
struct Tick {
    std::string symbol;
    double price;
    uint64_t volume;
    // std::chrono::system_clock::time_point timestamp;
    uint64_t timestamp;

    //class for simulation data procidures
    enum class Side {BID, ASK};
    Side side;

    //method to determine side(based on side)
    static Side determine_side(double price, double current_mid) {
        return price >= current_mid ? Side::ASK : Side::BID;
    }
};


