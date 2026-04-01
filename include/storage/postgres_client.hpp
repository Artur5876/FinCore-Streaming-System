#include <libpq-fe.h>
#include <chrono>
#include <variant>
#include <vector>
#include <unordered_map>
#include <string>
#include <optional>
#include <functional>

#include "core/types.hpp"

namespace fincore::db {

struct DbError {
    std::string message;
    std::string sqlstate; //5 digits normal
    std::string query; //failing query
};

template <typename T> using DbResult = std::variant<T, DbError>;

template <typename T> [[nodiscard]] bool            is_ok    (const DbResult<T>& r) noexcept { return std::holds_alternative<T>(r); }
template <typename T> [[nodiscard]] const T&        unwrap   (const DbResult<T>& r)           { return std::get<T>(r); }
template <typename T> [[nodiscard]] T&              unwrap   (DbResult<T>& r)                  { return std::get<T>(r); }
template <typename T> [[nodiscard]] const DbError&  error_of (const DbResult<T>& r)           { return std::get<DbError>(r); }

//asset_class[EQUITY, EXCHANGE, FUTURES OR ETFS]
struct Instrument {
    Symbol      symbol;
    std::string name, asset_class, exchange;
    int         tick_size_decimals{4}; //default: 4(int);
    bool is_active{true};
}


struct Candle {
    Symbol symbol;
    TimePoint bucket; //start of the interval

}


}

