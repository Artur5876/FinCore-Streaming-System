#include "/home/arturromanov/Documents/Financial-Core-Streaming-Project/include/redis_client.hpp"
#include <sstream>
#include <stdexcept>
#include <iostream>

//constructor with 127.0.0.1 IP address!!!
RedisClient::RedisClient(const std::string& host, int port)
    : redis("tcp://" + host + ":" + std::to_string(port)) {

    try {
        // Test the connection
        redis.ping();
        std::cout << "[Redis] Connected to " << host << ":" << port << "\n";
    } catch (const sw::redis::Error& e) {
        std::cerr << "[Redis] Connection failed: " << e.what() << "\n";
        // Don't exit immediately - maybe it's okay for testing
        // std::exit(1);
    }
}

void RedisClient::store_quote(const std::string& symbol, const AlphaVantageClient::Quote& quote) {
    try {
        //key for storing the latest quote data
        std::string quote_key = "quote:" + symbol;

        //hashmap for storing quote data
        std::unordered_map<std::string, std::string> quote_data = {
            {"symbol", std::to_string(quote.symbol)},
            {"price", std::to_string(quote.price)},
            {"open", std::to_string(quote.open)},
            {"high", std::to_string(quote.high)},
            {"low", std::to_string(quote.low)},
            {"volume", std::to_string(quote.volume)},
            {"change", std::to_string(quote.change)},
            {"change_percent", std::to_string(quote.change_percent)},
            {"timestamp", std::to_string(quote.timestamp)}
        };

        //store as hash
        redis.hmset(quote_key, quote_data.begin(), quote_data.end());

        //also add to historical redis list-structure with timestamp
        std::string historical_key = "quote_history:" + symbol;
        std::string historical_data = quote.timestamp + ":" +
            std::to_string(quote.price) +
            std::to_string(quote.volume);

        redis.lpush(historical_key, historical_data);

        //Limit: 1000 quotes in history
        redis.ltrim(historical_key, 0, 999);

        //Update last update timestamp
        std::string timestamp_key = "quote:" + symbol + ":updated";
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        redis.set(timestamp_key, std::to_string(timestamp));

        std::cout << "[REDIS] Stored quote for " << symbol << "\n";

    } catch(const sw::redis::Error& e) {
            std::cerr << "Error storing quote: " << e.what() << "\n";
    }
}

//retrieve struct from Redis
AlphaVantageClient::Quote RedisClient::get_quote(const std::string& symbol) {
   AlphaVantageClient::Quote quote;
    quote.symbol = symbol;

    try {
        std::string quote_key = "quote:" + symbol;

        // Get all fields from hash
        std::unordered_map<std::string, std::string> result;
        redis.hgetall(quote_key, std::inserter(result, result.begin()));

        if (!result.empty()) {
            try {
                if (result.find("price") != result.end())
                    quote.price = std::stod(result["price"]);
                if (result.find("open") != result.end())
                    quote.open = std::stod(result["open"]);
                if (result.find("high") != result.end())
                    quote.high = std::stod(result["high"]);
                if (result.find("low") != result.end())
                    quote.low = std::stod(result["low"]);
                if (result.find("volume") != result.end())
                    quote.volume = std::stoll(result["volume"]);
                if (result.find("change") != result.end())
                    quote.change = std::stod(result["change"]);
                if (result.find("change_percent") != result.end())
                    quote.change_percent = std::stod(result["change_percent"]);
                if (result.find("timestamp") != result.end())
                    quote.timestamp = result["timestamp"];
            } catch (const std::exception& e) {
                std::cerr << "[Redis] Error parsing quote data: " << e.what() << "\n";
            }
        }
    } catch (const sw::redis::Error& e) {
        std::cerr << "[Redis] Error retrieving quote: " << e.what() << "\n";
    }
    return quote;
}


// AlphaVantageClient::Quote RedisClient::get_last_tick(const std::string& symbol) {
//     try {
//         //Get data from Redis
//         auto result = redis.get("last_tick: " + symbol);
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
//     } catch(const sw::redis::Error& e) {
//         std::cout << "Error getting last element from Redis: " << e.what() << "\n";
//     }
//
//     return Tick{};//return empty list if nothing to return;
// }

//Store Tick in Redis Stream
void RedisClient::store_tick_stream(const Tick& tick) {
    try {
        std::string stream_key = "ticks_stream";

        //Field creation for relocation to Redis Stream
        std::unordered_map<std::string, std::string> fields = {
            {"price", std::to_string(tick.price)},
            {"volume", std::to_string(tick.volume)},
            {"side", (tick.side == Tick::Side::BID) ? "BID" : "ASK"}
        };


        //append to stream, '*' -> is auto generated ID
        this->redis.xadd(stream_key, "*", fields.begin(), fields.end());
    } catch (const sw::redis::Error& e) {
        std::cout << "Error storing tick stream: " << e.what() << "\n";
    }
}

// size_t RedisClient::get_stream_length(const std::string& symbol) {
//     try {
//         std::string stream_key = "ticks_stream";
//
//         //get stream length;
//         return redis.xlen(stream_key);
//     } catch (const sw::redis::Error& e) {
//         std::cout << "Error retriewing stream length: " << e.what() << "\n";
//         return 0;
//     }
// }

//Update order_book in redis
void RedisClient::updateOrderBook (const std::string& symbol,
                      const std::map<double, uint64_t>& bids,
                      const std::map<double, uint64_t>& asks) {
    try {
        //key of data
        std::string bids_key = "order_book:" + symbol + ":bids";
        std::string asks_key = "order_book:" + symbol + ":asks";

        //Clear existing data
        redis.del(bids_key);
        redis.del(asks_key);

        //store bids in hash Data str
        for (const auto& [price, volume] : bids) {
            std::string price_str = std::to_string(price);
            redis.hset(bids_key, price_str, std::to_string(volume));
        }

        //store asks in hash Data str
        for (const auto& [price, volume] : asks) {
            std::string price_str = std::to_string(price);
            redis.hset(asks_key, price_str, std::to_string(volume));
        }

        //Update the timestamp
        std::string timestamp_key = "orderbook:" + symbol + ":timestamp";
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

        //set updated timestamp in redis
        redis.set(timestamp_key, std::to_string(timestamp));
    } catch (const sw::redis::Error& e) {
        std::cerr << "Error updating order book: " << e.what() << "\n";
    }
}

