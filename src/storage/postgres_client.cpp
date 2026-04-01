#include <iostream>
#include "/home/arturromanov/untitled/Financial-Core-Streaming-System/include/storage/postgres_client.hpp"
namespace fincore::db {
namespace sql {

    constexpr auto kUpsertInstrument =
        "INSERT INTO instruments (symbol,name,asset_class,exchange,tick_size_decimals,is_active) "
        "VALUES ($1,$2,$3,$4,$5,$6) "
        "ON CONFLICT (symbol) DO UPDATE SET "
        "  name=EXCLUDED.name, asset_class=EXCLUDED.asset_class, exchange=EXCLUDED.exchange, "
        "  tick_size_decimals=EXCLUDED.tick_size_decimals, is_active=EXCLUDED.is_active, updated_at=NOW()";

    constexpr auto kSelectInstruments =
        "SELECT symbol,name,asset_class,exchange,tick_size_decimals,is_active "
        "FROM instruments ORDER BY symbol";

    constexpr auto kSelectInstrumentsActive =
        "SELECT symbol,name,asset_class,exchange,tick_size_decimals,is_active "
        "FROM instruments WHERE is_active=true ORDER BY symbol";











}
}
