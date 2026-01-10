#pragma once

#include <hiredis/hiredis.h>
#include <string>
#include <iostream>
#include <cstdlib>  // For exit()
#include "tick.hpp"
#include <map>
#include <sw/redis++/redis++.h>

class RedisClient {
private:
    
    sw::redis::Redis redis;
public:
    RedisClient(const std::string& host = "127.0.0.0", int port = 6379);
    // RedisClient() {
    //     context = redisConnect("127.0.0.1", 6379);
    //     if (context == nullptr || context->err) {
    //         if (context) {
    //             std::cerr << "Connection error: " << context->errstr << "\n";
    //             redisFree(context);
    //         } else {
    //             std::cerr << "Connection error: cannot allocate Redis context\n";
    //         }
    //         std::exit(1);
    //     }
    // }

    //Basec key-value
    void store_tick(const std::string& key, const Tick& tick); 
    //     // Add colon to key for better organization
    //     std::string key = "last_tick:" + tick.symbol;

    //     std::string value = std::to_string(tick.price) + "," +
    //                        std::to_string(tick.volume) + "," +
    //                        std::to_string(tick.timestamp);

    //     redisReply* reply = static_cast<redisReply*>(
    //         redisCommand(context, "SET %s %s", key.c_str(), value.c_str())
    //     );

    //     if (reply == nullptr) {
    //         std::cerr << "Redis command failed\n";
    //         return;
    //     }

    //     freeReplyObject(reply);
    // }

    Tick get_last_tick(const std::string& symbol); 
    //     std::string key = "last_tick:" + symbol;
    //     redisReply* reply = static_cast<redisReply*>(
    //         redisCommand(context, "GET %s", key.c_str())
    //     );

    //     Tick tick;
    //     tick.symbol = symbol;

    //     if (reply && reply->type == REDIS_REPLY_STRING) {
    //         std::string value = reply->str;
    //         size_t pos1 = value.find(',');
    //         size_t pos2 = value.find(',', pos1 + 1);

    //         // Fix: pos2 - pos1 - 1 (not pos2 - 1 - pos1)
    //         tick.price = std::stod(value.substr(0, pos1));
    //         tick.volume = std::stoi(value.substr(pos1 + 1, pos2 - pos1 - 1));
    //         tick.timestamp = std::stol(value.substr(pos2 + 1));
    //     }

    //     if (reply) {
    //         freeReplyObject(reply);
    //     }

    //     return tick;
    // }

    //Redis stream methods
    void store_tick_stream(const Tick& tick);
    size_t get_stream_length(const Tick& tick);

    //Order book storage(updation method)
    void updateOrderBook(const std::string& symbol,
                        const std::map<double, uint64_t>& bids,
                        const std::map<double, uint64_t>& asks);
    
};
