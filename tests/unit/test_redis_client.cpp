//Tests for Redis_Client logic (parsing, serialization)
//
//The tests logic is based on fake redis stub (no real Redis processing)
//
#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <unorderred_map>


//re-implementation of the types needed for testing


namespace fincore {
    using TimePoint = std::chrono::time_point<std::chrono::system_clock,
                                          std::chrono::microseconds>;
    using Symbol = std::string;
    using Price  = double;
    using Volume = uint64_t;

    enum class Side : char { Bid = 'B', Ask = 'A', Unknown = 'U' };

    inline std::string_view to_string(Side s) noexcept {
        switch (s) {
            case Side::Bid: return "BID";
            case Side::Ask: return "ASK";
            default:        return "UNKNOWN";
        }
    }

    inline int64_t to_unix_us(TimePoint tp) noexcept {
        return tp.time_since_epoch().count();
    }

    struct Quote {
        Symbol      symbol;
        Price       price{};
        Price       open{};
        Price       high{};
        Price       low{};
        Volume      volume{};
        double      change_pct{};
        std::string source{"ALPHA_VANTAGE"};
        TimePoint   timestamp{};

        [[nodiscard]] bool is_valid() const noexcept {
            return !symbol.empty() && price > 0.0;
        }
    };

    struct Tick {
        Symbol      symbol;
        Price       price{};
        Volume      size{};
        Side        side{Side::Unknown};
        std::string source{"STREAM"};
        TimePoint   timestamp{};

        [[nodiscard]] bool is_valid() const noexcept {
            return !symbol.empty() && price > 0.0 && size > 0;
        }
    };


    //Simple logic helpers that help to extract data from Redis_Client. those methods are exactly what
    //Redis_Client does when serialising/desirialising, so we can test the logic without touching the network
}
namespace fincore::detail {
    //I will serialise a Quote into a flat string map (what hset receives)
    std::unordered_map<std::string, std::string> quote_to_fields(const Quote& q) {
        return {
            {"price",      std::to_string(q.price)},
            {"open",       std::to_string(q.open)},
            {"high",       std::to_string(q.high)},
            {"low",        std::to_string(q.low)},
            {"volume",     std::to_string(q.volume)},
            {"change_pct", std::to_string(q.change_pct)},
            {"source",     q.source},
            {"timestamp",  std::to_string(to_unix_us(q.timestamp))},
        };
    }

    //desirialising a string map back to Quote
    std::optional<Quote> fields_to_quote(const Symbol& symbol,
                                     const std::unordered_map<std::string, std::string>& fields)
    {
        if (fields.empty()) return std::nullopt;   // BUG FIX: was inverted in original

        auto get_double = [&](const std::string& key) -> double {
            auto it = fields.find(key);
            if (it == fields.end()) return 0.0;
            try { return std::stod(it->second); } catch (...) { return 0.0; }
        };
        auto get_uint64 = [&](const std::string& key) -> uint64_t {
            auto it = fields.find(key);
            if (it == fields.end()) return 0ULL;
            try { return std::stoull(it->second); } catch (...) { return 0ULL; }
        };

        Quote q;
        q.symbol     = symbol;
        q.price      = get_double("price");
        q.open       = get_double("open");
        q.high       = get_double("high");
        q.low        = get_double("low");
        q.volume     = get_uint64("volume");
        q.change_pct = get_double("change_pct");

        if (auto it = fields.find("source"); it != fields.end())
            q.source = it->second;

        if (auto it = fields.find("timestamp"); it != fields.end()) {
            try {
                int64_t us = std::stoll(it->second);
                q.timestamp = TimePoint(std::chrono::microseconds(us));
            } catch (...) {}
        }

        return q;
    }

    //Serialise tick to fields
    std::unordered_map<std::string, std::string> tick_to_fields(const Tick& t) {
        return {
            {"symbol",    t.symbol},
            {"price",     std::to_string(t.price)},
            {"size",      std::to_string(t.size)},
            {"side",      std::string(to_string(t.side))},
            {"source",    t.source},
            {"timestamp", std::to_string(to_unix_us(t.timestamp))},
        };
    }
}
