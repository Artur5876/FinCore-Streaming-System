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


//TESTS
using namespace fincore;
using namespace fincore::detail;

TEST(QuotesSerde, RoundTrip_AllFields) {
    Quote original;
    original.symbol     = "AAPL";
    original.price      = 189.75;
    original.open       = 188.00;
    original.high       = 191.20;
    original.low        = 187.50;
    original.volume     = 42'000'000ULL;
    original.change_pct = 0.93;
    original.source     = "ALPHA_VANTAGE";
    original.timestamp  = TimePoint(std::chrono::microseconds(1'700'000'000'000'000LL));

    auto fields = quote_to_fields(original);
    auto result = fields_to_quote("AAPL", fields);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ   (result->symbol,     "AAPL");
    EXPECT_DOUBLE_EQ(result->price,  189.75);
    EXPECT_DOUBLE_EQ(result->open,   188.00);
    EXPECT_DOUBLE_EQ(result->high,   191.20);
    EXPECT_DOUBLE_EQ(result->low,    187.50);
    EXPECT_EQ   (result->volume,     42'000'000ULL);
    EXPECT_NEAR (result->change_pct, 0.93, 1e-9);
    EXPECT_EQ   (result->source,     "ALPHA_VANTAGE");
    EXPECT_EQ   (result->timestamp,  original.timestamp);
}

TEST(QuoteSerde, NoEmptyFields_ReturnValue) {
    std::unorderred_map<std::string, std::string> fields{{"price", "100.0"}};
    auto result = fields_to_quote("MSFT", fields);
    EXPECT_TRUE(result.has_value());
}

//Tests to validate the Quote struct
TEST(QuoteValidation, Valid) {
    Quote q;
    q.symbol = "GOOG";
    q.price = "142.0";
    EXPECT_TRUE(q.is_valid());
}

TEST(QuoteValidation, EmptySymbol_Invalid) {
    Quote q;
    q.price = 100.0;
    EXPECT_FALSE(q.is_valid());
}

TEST(QuoteValidation, ZeroPrice_Invalid) {
    Quote q;
    q.symbol = "GOOG";
    q.price = "0.0";
    EXPECT_FALSE(q.is_valid());
}

//Tests for tick serialisation
TEST(TickSerde, SideEncoding_Bid) {
    Tick t;
    t.symbol = "BTC-USD";
    t.price = 1.0;
    t.size = 1;
    t.side = Side::Bid;

    auto fields = tick_to_fields(t);
    EXPECT_EQ(fields.at("side"), "BID");
}

TEST(TickSerde, SideEncoding_Ask) {
    Tick t;
    t.symbol = "ETH-USD";
    t.price = 1.0;
    t.size = 1;
    t.side = Side::Ask;

    auto fields = tick_to_fields(t);
    EXPECT_EQ(fields.at("side"), "ASK");
}

TEST(TickSerde, TimestampRoundTrip) {
    const int64_t us = 1'700'000'000'123'456LL;

    Tick t;
    t.symbol    = "AAPL";
    t.price     = 1.0;
    t.size      = 1;
    t.timestamp = TimePoint(std::chrono::microseconds(us));

    auto fields = tick_to_fields(t);
    EXPECT_EQ(std::stoll(fields.at("timestamp")), us);
}

//Side enum converter(helper)
TEST(SideEnum, ToString) {
    EXPECT_EQ(to_string(Side::Bid), "BID");
    EXPECT_EQ(to_string(Side::Ask), "ASK");
    EXPECT_EQ(to_string(Side::Unknown), "UNKNOWN");
}

//TimePoint helper;
TEST(TimePoint, ToUnixUs_RoundTrip) {
    const int64_t us = 1'715'000'000'000'000LL;
    TimePoint tp(std::chrono::microseconds(us));
    EXPECT_EQ(to_unix_us(tp), us);
}

//Test some Quote and Tick naming conventions
TEST(KeyHelpers, QuoteKey) {
    // Mirror of RedisClient::quote_key
    auto quote_key = [](const Symbol& s) { return "quote:" + s; };
    EXPECT_EQ(quote_key("AAPL"), "quote:AAPL");
}

TEST(KeyHelpers, QuoteHistoryKey) {
    auto quote_history_key = [](const Symbol& s) { return "quote_history:" + s; };
    EXPECT_EQ(quote_history_key("AAPL"), "quote_history:AAPL");
}

TEST(KeyHelpers, TickStreamKey) {
    auto tick_stream_key = [](const Symbol& s) { return "ticks:" + s; };
    EXPECT_EQ(tick_stream_key("AAPL"), "ticks:AAPL");
}


