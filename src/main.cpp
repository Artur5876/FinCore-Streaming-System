#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <chrono>

struct Tick {
    std::string symbol;
    double price;
    size_t volume;
    long timestamp;
};

Tick parseCSVLine(const std::string& line) {
    Tick tick;
    //finding the first index in the line(after coma)
    size_t pos1 = line.find(",");
    size_t pos2 = line.find(",", pos1);
    size_t pos3 = line.find(",", pos2);
    size_t pos4 = line.find(",", pos3);
    
    tick.symbol =line.substr(0,pos1);
    tick.price = std::stod(line.substr(pos2+1, pos2 -1 -pos1));
    tick.volume = std::stoi(line.substr(pos3 + 1, pos3 - 1 - pos2));
    tick.timestamp = std::stol(line.substr(pos4 + 1, pos4 - 1 - pos3));
}
int main() {
    std::cout << "=======================================\n";
    std::cout << "MARKET DATA PIPELINE v0.1 - DAY 1\n";
    std::cout << "=======================================\n\n";
    auto currentTime = std::chrono::high_resolution_clock::now();
    std::cout <<  "14.30 Initializing system...\n";

    return 0;
}
