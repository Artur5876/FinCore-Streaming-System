#pragma once
#include <chrono>
#include <string>

namespace fincore
{
    struct Quote {
        std::string symbol;
        double price{};
        double open{};
        double high{};
        double low{};
        std::uint64_t volume{};
        double change_pct{};
        std::string source;
        std::chrono::system_clock::time_point timestamp{};
    };
}