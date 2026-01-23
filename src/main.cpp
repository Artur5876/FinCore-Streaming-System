#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include "/home/arturromanov/Documents/Financial-Core-Streaming-Project/src/redis_client.cpp"
#include <chrono>
#include "/home/arturromanov/Documents/Financial-Core-Streaming-Project/include/tick.hpp";
#include <sstream>

Tick parseCSVLine(const std::string& line) {
    Tick tick;
    std::stringstream ss(line);
    std::string token;

    //skip the first field (symbol)
    std::getline(ss, tick.symbol, ',');

    //skip the exchange field
    std::getline(ss, token, ',');

    //parse price
    std::getline(ss, token, ',');
    tick.price = std::stod(token);

    //parse volume
    std::getline(ss, token, ',');
    tick.volume = std::stoi(token);

    //parse timestamp
    std::getline(ss, token);
    tick.timestamp = std::stoi(token);

    return tick;
}
int main() {
    std::cout << "=======================================\n";
    std::cout << "MARKET DATA PIPELINE v0.1 - DAY 1\n";
    std::cout << "=======================================\n\n";

    std::cout << "14.30 Initializing system...\n";

    // Redis connection
    RedisClient redis;
    std::cout << "Connecting to redis server: localhost:6379\n";

    // CSV Loading
    std::ifstream file("/home/arturromanov/Documents/Financial-Core-Streaming-Project/data/sample_data.csv");
    if (!file.is_open()) {
        std::cerr << "ERROR: Cannot open sample_data.csv" << std::endl;
        std::cerr << "Current directory: ";
        system("pwd");
        std::cerr << "Files in directory: ";
        system("ls -la");
        return 1;
    }

    std::string line;
    std::getline(file, line); // Skip header
    std::cout << "[14:30:03] Loading sample data: sample_data.csv\n\n";
    std::cout << "=== PROCESSING START ===\n";

    // Processing first line only
    if (std::getline(file, line)) {
        try {
            Tick tick = parseCSVLine(line);

            std::cout << "Parsing tick N1: " << tick.symbol
                      << ", $" << tick.price
                      << ", " << tick.volume
                      << " shares, " << tick.timestamp << "\n";

            // Store in Redis
            redis.storeTick(tick);
            std::cout << "[14:30:04] Stored to Redis Stream: ticks:" << tick.symbol << "\n";

            // Verify (read back)
            Tick lastTick = redis.getLastTick(tick.symbol);
            std::cout << "\n=== VERIFICATION ===\n";
            std::cout << "Redis Check:\n";
            std::cout << "  - Last tick for " << lastTick.symbol
                      << ": $" << lastTick.price
                      << " (" << lastTick.volume << " shares)\n";
        } catch (const std::exception& e) {
            std::cerr << "ERROR: " << e.what() << std::endl;
            std::cerr << "Line that failed: " << line << std::endl;
            return 1;
        }
    } else {
        std::cerr << "ERROR: No data in CSV file\n";
        return 1;
    }

    std::cout << "\n DAY 1 COMPLETE: System processes 1 tick end-to-end\n";
    return 0;
}