#include "/home/arturromanov/Documents/Financial-Core-Streaming-Project/include/storage/redis_client.hpp"
#include <sstream>
#include "/home/arturromanov/Documents/Financial-Core-Streaming-Project/include/api/alpha_vantage_client.hpp"
#include <optional>
#include <iomanip>
#include <map>


namespace fincore {
//constructor with 127.0.0.1 IP address!!!
RedisClient::RedisClient(const std::string& host, int port)
    : connection_string_("tcp://" + host + ":" + std::to_string(port)) {

    try {
        redis_ =  std::make_unique<sw::redis::Redis>(connection_string_);
        //test the connection
        redis_->ping();
        std::cout << "[Redis] Connected to " << host << ":" << port << "\n";
    } catch (const sw::redis::Error& e) {
        std::cerr << "[Redis] Connection failed: " << e.what() << "\n";
        throw;
    }
}

bool RedisClient::store_quote(const Symbol& symbol, const Quote& quote) {
    try {
        //hashmap for storing quote data
        std::unordered_map<std::string, std::string> quote_data = {
            {"price", std::to_string(quote.price)},
            {"open", std::to_string(quote.open)},
            {"high", std::to_string(quote.high)},
            {"low", std::to_string(quote.low)},
            {"volume", std::to_string(quote.volume)},
            {"change", std::to_string(quote.change)},
            {"change_percent", std::to_string(quote.change_percent)},
            {"timestamp", quote.timestamp}
        };

        //store as hash
        redis_->hmset(quote_key(symbol), quote_data.begin(), quote_data.end());

        //also add to historical redis_->list-structure with timestamp
        std::string historical_key = "quote_history:" + symbol;
        std::string historical_data = quote.timestamp + ":" +
            std::to_string(quote.price) + ":" +
            std::to_string(quote.volume);

        redis_->lpush(quote_history_key(symbol), historical_data);
        //limit: 1000 quotes in history
        redis_->ltrim(historical_key, 0, 999);

        return true;
    } catch(const sw::redis::Error& e) {
            std::cerr << "Error storing quote: " << e.what() << "\n";
            return false;
    }
}




//retrieve struct from Redis
std::optional<Quote> RedisClient::get_quote(const Symbol& symbol) {
    try {
        //get all fields from hash
        std::unordered_map<std::string, std::string> result;
        redis_->hgetall(quote_key(symbol), std::inserter(result, result.begin()));

        if (!result.empty()) {
            return std::nullopt;
        }

        Quote quote;
        quote.symbol = symbol;

        //safer parsing with error checking
        auto get_double = [&](const std::string& key) -> double {

            auto it = result.find(key);
            return (it != result.end()) ? std::stoull(it->second) : 0ULL;
        };

        auto get_uint64 = [&](const std::string& key) -> uint64_t {
            auto it = result.find(key);
            return (it != result.end()) ? std::stoull(it->second) : 0ULL;
        };

        quote.price = get_double("price");
        quote.open = get_double("open");
        quote.high = get_double("high");
        quote.low = get_double("low");
        quote.volume = get_uint64("volume");
        quote.change = get_double("change");
        quote.change_percent = get_double("change_percent");

        auto it = result.find("timestamp");
        if (it != result.end()) quote.timestamp = it->second;

        return quote;
    } catch(const std::exception& e) {
        std::cerr << "[Redis] Error parsing quote: " << e.what() << "\n";
        return std::nullopt;
    }
}


bool RedisClient::store_tick(const Tick& tick) {
    try {
        std::unordered_map<std::string, std::string> fields = {
            {"symbol", tick.symbol},
            {"price", std::to_string(tick.price)},
            {"volume", std::to_string(tick.volume)},
            {"side", (tick.side == Side::BID) ? "BID" : "ASK"},
            {"timestamp", std::to_string(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    tick.timestamp.time_since_epoch()).count())}
        };

        redis_->xadd(tick_stream_key(tick.symbol), "*", fields.begin(), fields.end());

        //latest tick update
        redis_->set("latest_tick:" + tick.symbol, std::to_string(tick.price) +
                ":" + std::to_string(tick.volume));
        return true;
    } catch (const sw::redis::Error& error) {
        std::cerr << "[Redis] Error storing tick: " << error.what() << "\n";
        return false;
    }
}

void RedisClient::update_order_book(const Symbol& symbol,
                                    const std::map<Price, Volume>& bids,
                                    const std::map<Price, Volume>& asks) {
    try {
        std::string bids_key = "order_book:" + symbol + ":bids";
        std::string asks_key = "order_book:" + symbol + ":asks";

        redis_->del(bids_key);
        redis_->del(asks_key);

        for (const auto& [price, volume]: bids) {
            redis_->hset(bids_key, std::to_string(price), std::to_string(volume));
        }

        for (const auto& [price, volume] : asks) {
            redis_->hset(asks_key, std::to_string(price), std::to_string(volume));
        }

        redis_->set("orderbook:" + symbol + ":timestamp",
                std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count()));
    } catch (const sw::redis::Error& e) {
        std::cerr << "[Redis] Error updating order book: " << e.what() << "\n";
    }
}

// AlphaVantageClient::Quote RedisClient::get_last_tick(const std::string& symbol) {
//     try {
//         //Get data from Redis
//         auto result = redis_->get("last_tick: " + symbol);
//
//         //if the data is found
//         if (result) {
//             //parsing the string back to Tick struct
//             std::string data = *result;
//             Tick tick;
//
//             //simple parsing data format(price, volume, side)
//             size_t pos1 = data.find(',');
//             size_t pos2 = data.find(',', pos1 + 1);
//
//             if (pos1 != std::string::npos && pos2 != std::string::npos) {
//                 tick.price = std::stod(data.substr(0, pos1));
//                 tick.volume = std::stoull(data.substr(pos1 + 1, pos2 - pos1 - 1));
//                 std::string side_str = data.substr(pos2 + 1);
//                 tick.side = (side_str == "BID") ? Tick::Side::BID : Tick::Side::ASK;
//             }
//             return tick;
//         }
//
//     } catch(const sw::redis_->:Error& e) {
//         std::cout << "Error getting last element from Redis: " << e.what() << "\n";
//     }
//
//     return Tick{};//return empty list if nothing to return;
// }
// size_t RedisClient::get_stream_length(const std::string& symbol) {
//     try {
//         std::string stream_key = "ticks_stream";
//
//         //get stream length;
//         return redis_->xlen(stream_key);
//     } catch (const sw::redis_->:Error& e) {
//         std::cout << "Error retriewing stream length: " << e.what() << "\n";
//         return 0;
//     }
// }

//Update order_book in redis_
bool RedisClient::is_connected() const {
    try {
        redis_->ping();
        return true;
    } catch(...) {
        return false;
    }
}

//helper-func;
std::string RedisClient::quote_key(const Symbol& symbol) const {
    return "quote:" + symbol;
}

std::string RedisClient::quote_history_key(const Symbol& symbol) const {
    return "quote_history:" + symbol;
}

std::string RedisClient::tick_stream_key(const Symbol& symbol) const {
    return "ticks:" + symbol;
}

};
