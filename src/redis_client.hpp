#pragma once

#include <hiredis/hiredis.h>
#include <string>
#include <iostream>
#include <cstdlib>  // For exit()
#include "tick.hpp"

// Remove: struct Tick;  // NOT NEEDED since you include tick.hpp

class RedisClient {
private:
    redisContext* context;

public:
    RedisClient() {
        context = redisConnect("127.0.0.1", 6379);
        if (context == nullptr || context->err) {
            if (context) {
                std::cerr << "Connection error: " << context->errstr << "\n";
                redisFree(context);
            } else {
                std::cerr << "Connection error: cannot allocate Redis context\n";
            }
            std::exit(1);
        }
    }

    void storeTick(const Tick& tick) {
        // Add colon to key for better organization
        std::string key = "last_tick:" + tick.symbol;

        std::string value = std::to_string(tick.price) + "," +
                           std::to_string(tick.volume) + "," +
                           std::to_string(tick.timestamp);

        redisReply* reply = static_cast<redisReply*>(
            redisCommand(context, "SET %s %s", key.c_str(), value.c_str())
        );

        if (reply == nullptr) {
            std::cerr << "Redis command failed\n";
            return;
        }

        freeReplyObject(reply);
    }

    Tick getLastTick(const std::string& symbol) {
        std::string key = "last_tick:" + symbol;
        redisReply* reply = static_cast<redisReply*>(
            redisCommand(context, "GET %s", key.c_str())
        );

        Tick tick;
        tick.symbol = symbol;

        if (reply && reply->type == REDIS_REPLY_STRING) {
            std::string value = reply->str;
            size_t pos1 = value.find(',');
            size_t pos2 = value.find(',', pos1 + 1);

            // Fix: pos2 - pos1 - 1 (not pos2 - 1 - pos1)
            tick.price = std::stod(value.substr(0, pos1));
            tick.volume = std::stoi(value.substr(pos1 + 1, pos2 - pos1 - 1));
            tick.timestamp = std::stol(value.substr(pos2 + 1));
        }

        if (reply) {
            freeReplyObject(reply);
        }

        return tick;
    }

    ~RedisClient() {
        if (context) {
            redisFree(context);
        }
    }
};
