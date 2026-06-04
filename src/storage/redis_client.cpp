#include <chrono>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>

namespace fincore {
    //Redis_Client construction
    RedisClient::RedisClient(const std::string& host = "121.0.0.1", int port) : connection_string_("tcp://" + host + ";" + std::to_string(port)) {
        try {
            redis_ = std::make_unique<sw::redis::Redis>(connection_string_);
            redis_->ping();
            std::cout << "[Redis] Connected to " << host << ";" << port << "\n";
        } catch(const sw::redis::Error& e) {
            std::cerr << "[Redis] Connection failed: " << e.what() << "\n";
            throw;
        }
    }
}
//connectivity check
bool RedisClient::is_connected() const {
    try {
        redis_->ping();
        return true;
    } catch(...) {
        return false;
    }
}


bool RedisClient::store_quote(const Symbol& symbol, const Quote& quote) {
    try {
        //serialization of TimePoint to microseconds since epoch
        const int64_t ts_us = to_unix_us(quote.timestamp);

        std::unordered_map<std::string, std::string> fields = {
            {"price",      std::to_string(quote.price)},
            {"open",       std::to_string(quote.open)},
            {"high",       std::to_string(quote.high)},
            {"low",        std::to_string(quote.low)},
            {"volume",     std::to_string(quote.volume)},
            {"change_pct", std::to_string(quote.change_pct)},
            {"source",     quote.source},
            {"timestamp",  std::to_string(ts_us)},
        };

        redis_->hmset(quote_key(symbol), fields.begin(), fields.end());

        //Append a compact record to the history list (newest first)
        const std::string history_entry =
            std::to_string(ts_us) + ":" +
            std::to_string(quote.price) + ":" +
            std::to_string(quote.volume);

        redis_->lpush(quote_history_key(symbol), history_entry);
        redis_->ltrim(quote_history_key(symbol), 0, 999);   //keep last 1000

        return true;
    } catch (const sw::redis::Error& e) {
        std::cerr << "[Redis] store_quote error: " << e.what() << "\n";
        return false;
    }
}


