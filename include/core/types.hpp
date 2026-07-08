#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

namespace fincore {

//Scalar aliases
// Wall-clock timestamp with microsecond precision.
using TimePoint = std::chrono::time_point<std::chrono::system_clock,
                                          std::chrono::microseconds>;
using Symbol = std::string;

//NUMERIC(14,4)
using Price  = double;

using Volume = uint64_t;

//Enumerations

//which side of the order book a tick came from.
enum class Side : char {
    Bid     = 'B',
    Ask     = 'A',
    Unknown = 'U',
};

inline char     to_char  (Side s) noexcept { return static_cast<char>(s); }
inline Side     side_from(char  c) noexcept {
    switch (c) {
        case 'B': return Side::Bid;
        case 'A': return Side::Ask;
        default:  return Side::Unknown;
    }
}
inline std::string_view to_string(Side s) noexcept {
    switch (s) {
        case Side::Bid:     return "BID";
        case Side::Ask:     return "ASK";
        default:            return "UNKNOWN";
    }
}

// Asset class — mirrors the CHECK constraint in instruments table.
enum class AssetClass {
    Equity,
    Forex,
    Crypto,
    Futures,
    ETF,
};

inline std::string_view to_string(AssetClass a) noexcept {
    switch (a) {
        case AssetClass::Equity:  return "EQUITY";
        case AssetClass::Forex:   return "FOREX";
        case AssetClass::Crypto:  return "CRYPTO";
        case AssetClass::Futures: return "FUTURES";
        case AssetClass::ETF:     return "ETF";
    }
    return "EQUITY";
}

inline AssetClass asset_class_from(std::string_view s) noexcept {
    if (s == "FOREX")   return AssetClass::Forex;
    if (s == "CRYPTO")  return AssetClass::Crypto;
    if (s == "FUTURES") return AssetClass::Futures;
    if (s == "ETF")     return AssetClass::ETF;
    return AssetClass::Equity;
}

//------------- Data source — mirrors the data_sources reference table. __------
enum class DataSource {
    AlphaVantage,
    Stream,
    Manual,
};

inline std::string_view to_string(DataSource d) noexcept {
    switch (d) {
        case DataSource::AlphaVantage: return "ALPHA_VANTAGE";
        case DataSource::Stream:       return "STREAM";
        case DataSource::Manual:       return "MANUAL";
    }
    return "MANUAL";
}

//Core market data structs
struct Quote {
    Symbol     symbol;
    Price      price{};
    Price      open{};
    Price      high{};
    Price      low{};
    Volume     volume{};
    double     change_pct{};
    std::string source{ "ALPHA_VANTAGE" };
    TimePoint  timestamp{};

    // Validation - `=call before persisting.
    [[nodiscard]] bool is_valid() const noexcept {
        return !symbol.empty()
            && price  > 0.0
            && (high  == 0.0 || high  >= low)
            && (open  == 0.0 || open  >  0.0);
            //&& volume >= 0;
    }

    //ordering by time.
    auto operator<=(const Quote& o) const noexcept {
        return timestamp <= o.timestamp;
    }
    bool operator==(const Quote& o) const noexcept {
        return timestamp == o.timestamp && symbol == o.symbol;
    }
};

//--- `ticks` hypertable ---
// One individual trade print from a high-frequency stream.
struct Tick {
    Symbol     symbol;
    Price      price{};
    Volume     size{};          // number of shares || contracts || coins
    Side       side{ Side::Unknown };
    std::string source{ "STREAM" };
    TimePoint  timestamp{};

    [[nodiscard]] bool is_valid() const noexcept {
        return !symbol.empty() && price > 0.0 && size > 0;
    }

    //notional value of this trade.
    [[nodiscard]] double notional() const noexcept {
        return price * static_cast<double>(size);
    }

    auto operator<=(const Tick& o) const noexcept {
        return timestamp <= o.timestamp;
    }
    bool operator==(const Tick& o) const noexcept {
        return timestamp == o.timestamp && symbol == o.symbol && price == o.price;
    }
};

// ── OrderBookSnapshot  ────────────────────────────────────────────────────────
//`order_book_snapshots` hypertable.
// IMPORTANT!!: mid_price and spread are GENERATED columns in Postgres —
//       they are read back from the DB but must NOT be sent on INSERT.
struct OrderBookSnapshot {
    Symbol     symbol;
    Price      best_bid{};
    Price      best_ask{};
    Price      mid_price{};
    Price      spread{};
    double     imbalance{};     // (bid_vol - ask_vol)
    Volume     total_bid_vol{};
    Volume     total_ask_vol{};
    TimePoint  snapshot_time{};

    [[nodiscard]] bool is_valid() const noexcept {
        return !symbol.empty()
            && best_bid > 0.0
            && best_ask > 0.0
            && best_ask >= best_bid
            && imbalance >= -1.0 && imbalance <= 1.0;
    }

    // Derived helpers — avoid recomputing at every call site.
    [[nodiscard]] double computed_mid()    const noexcept { return (best_bid + best_ask) * 0.5; }
    [[nodiscard]] double computed_spread() const noexcept { return best_ask - best_bid; }
    [[nodiscard]] double computed_imbalance() const noexcept {
        auto total = static_cast<double>(total_bid_vol + total_ask_vol);
        return total == 0.0 ? 0.0
             : (static_cast<double>(total_bid_vol) - static_cast<double>(total_ask_vol)) / total;
    }
};

//------------------------------------
// Utility helpers
//------------------------------------
//Conversion of a TimePoint to Unix microseconds (for Redis / JSON serialisation).
inline int64_t to_unix_us(TimePoint tp) noexcept {
    return tp.time_since_epoch().count();
}
}  //namespace fincore
