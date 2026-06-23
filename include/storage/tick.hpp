// #pragma once
//
// #include <string>
// #include <chrono>
// #include <map>
//
// namespace fincore {
//
// //strong types for better code clarity
// using Price = double;
// using Volume = uint64_t;
// using Symbol = std::string;
// using TimePoint = std::chrono::system_clock::time_point;
//
// enum class Side { BID, ASK };
//
// struct Tick {
//     Symbol symbol;
//     Price price;
//     Volume volume;
//     Side side;
//     std::chrono::system_clock::time_point timestamp;
//
//     Tick(Symbol sym, Price p, Volume v, Side s)
//         : symbol(std::move(sym))
//         , price(p)
//         , volume(v)
//         , side(s)
//         , timestamp(std::chrono::system_clock::now()) {}
// };
//
// //move from alpha_vantage_client
// struct Quote {
//     Symbol symbol;
//     Price price;
//     Price open;
//     Price high;
//     Price low;
//     Volume volume;
//     double change;
//     double change_percent;
//     std::chrono::microseconds timestamp;
// };
//
//     struct OrderBookSnapshot {
//     Symbol symbol;
//     TimePoint snapshot_time;
//     double best_bid;
//     double best_ask;
//     double mid_price;
//     double spread;
//     Volume total_bid_vol;
//     Volume total_ask_vol;
//     double imbalance;
// };
// }
