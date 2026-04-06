<<<<<<< Updated upstream
#include <iostream>
#include "include/storage/postgres_client.hpp"

namespace fincore::db {
namespace sql {
=======
// =============================================================================
// fincore/db/postgres_client.cpp
// =============================================================================
#include "db/postgres_client.hpp"
>>>>>>> Stashed changes

#include <array>
#include <cassert>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>

#include <fmt/format.h>

namespace fincore::db {

// =============================================================================
// § A  SQL constants  — keep queries out of method bodies
// =============================================================================
namespace sql {
    // ── Instruments ──────────────────────────────────────────────────────────
    constexpr auto kUpsertInstrument =
        "INSERT INTO instruments (symbol,name,asset_class,exchange,tick_size_decimals,is_active) "
        "VALUES ($1,$2,$3,$4,$5,$6) "
        "ON CONFLICT (symbol) DO UPDATE SET "
        "  name=EXCLUDED.name, asset_class=EXCLUDED.asset_class, exchange=EXCLUDED.exchange, t"
        "  exchange=EXCLUDED.exchange, tick_size_decimals=EXCLUDED.tick_size_decimals, "
        "  is_active=EXCLUDED.is_active, updated_at=NOW()";

    constexpr auto kSelectInstruments =
        "SELECT symbol,name,asset_class,exchange,tick_size_decimals,is_active "
        "FROM instruments ORDER BY symbol";

    constexpr auto kSelectInstrumentsActive =
        "SELECT symbol,name,asset_class,exchange,tick_size_decimals,is_active "
        "FROM instruments WHERE is_active=true ORDER BY symbol";

    constexpr auto kFindInstrument =
        "SELECT symbol,name,asset_class,exchange,tick_size_decimals,is_active "
        "FROM instruments WHERE symbol=$1";

    constexpr auto kSetInstrumentActive =
        "UPDATE instruments SET is_active=$2, updated_at=NOW() WHERE symbol=$1";

    // ── Quotes ───────────────────────────────────────────────────────────────
    constexpr auto kInsertQuote =
        "INSERT INTO quotes (symbol,price,open,high,low,volume,change_pct,source,timestamp) "
        "VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9)";

    constexpr auto kSelectQuotes =
        "SELECT symbol,price,open,high,low,volume,change_pct,source,timestamp "
        "FROM quotes WHERE symbol=$1 AND timestamp BETWEEN $2 AND $3 "
        "ORDER BY timestamp DESC LIMIT $4";

    constexpr auto kLatestPrice =
        "SELECT price FROM quotes WHERE symbol=$1 ORDER BY timestamp DESC LIMIT 1";

    constexpr auto kLatestPrices =
        "SELECT DISTINCT ON (symbol) symbol, price FROM quotes "
        "WHERE symbol = ANY($1) ORDER BY symbol, timestamp DESC";

    // ── Ticks ────────────────────────────────────────────────────────────────
    constexpr auto kInsertTick =
        "INSERT INTO ticks (symbol,price,size,side,source,timestamp) "
        "VALUES ($1,$2,$3,$4,$5,$6)";

    constexpr auto kSelectTicks =
        "SELECT symbol,price,size,side,source,timestamp FROM ticks "
        "WHERE symbol=$1 AND timestamp BETWEEN $2 AND $3 "
        "ORDER BY timestamp DESC LIMIT $4";

    constexpr auto kCountTicks =
        "SELECT COUNT(*) FROM ticks WHERE symbol=$1 AND timestamp BETWEEN $2 AND $3";

    constexpr auto kVwap =
        "SELECT SUM(price*size)/NULLIF(SUM(size),0) FROM ticks "
        "WHERE symbol=$1 AND timestamp BETWEEN $2 AND $3";

    constexpr auto kCandles =
        "SELECT symbol, time_bucket($1::interval,timestamp) AS bucket, "
        "FIRST(price,timestamp), MAX(price), MIN(price), LAST(price,timestamp), "
        "SUM(size), COUNT(*) "
        "FROM ticks WHERE symbol=$2 AND timestamp BETWEEN $3 AND $4 "
        "GROUP BY symbol,bucket ORDER BY bucket DESC LIMIT $5";

    constexpr auto kDailyOhlcv =
        "SELECT symbol,bucket,open_price,high_price,low_price,close_price,total_volume,tick_count "
        "FROM daily_ohlcv WHERE symbol=$1 AND bucket BETWEEN $2::date AND $3::date "
        "ORDER BY bucket DESC LIMIT $4";

    constexpr auto kStreamTicks =
        "SELECT symbol,price,size,side,source,timestamp FROM ticks "
        "WHERE symbol=$1 AND timestamp > $2 ORDER BY timestamp ASC LIMIT $3";

    // ── Order book ───────────────────────────────────────────────────────────
    constexpr auto kInsertSnapshot =
        "INSERT INTO order_book_snapshots "
        "(symbol,best_bid,best_ask,imbalance,total_bid_vol,total_ask_vol,snapshot_time) "
        "VALUES ($1,$2,$3,$4,$5,$6,NOW())";

    constexpr auto kLatestSnapshot =
        "SELECT symbol,best_bid,best_ask,mid_price,spread,imbalance,"
        "total_bid_vol,total_ask_vol,snapshot_time "
        "FROM order_book_snapshots WHERE symbol=$1 "
        "ORDER BY snapshot_time DESC LIMIT 1";

    constexpr auto kSpreadStats =
        "SELECT AVG(spread),MIN(spread),MAX(spread),AVG(imbalance),AVG(mid_price),COUNT(*) "
        "FROM order_book_snapshots WHERE symbol=$1 AND snapshot_time BETWEEN $2 AND $3";

    constexpr auto kHourlySummary =
        "SELECT bucket,avg_spread,min_spread,max_spread,avg_imbalance,avg_mid "
        "FROM hourly_book_summary WHERE symbol=$1 AND bucket BETWEEN $2::date AND $3::date "
        "ORDER BY bucket DESC";

    // ── Indicators ───────────────────────────────────────────────────────────
    constexpr auto kInsertIndicator =
        "INSERT INTO technical_indicators (symbol,indicator_name,value,parameters,timestamp) "
        "VALUES ($1,$2,$3,$4::jsonb,NOW())";

    constexpr auto kLastIndicator =
        "SELECT value FROM technical_indicators "
        "WHERE symbol=$1 AND indicator_name=$2 ORDER BY timestamp DESC LIMIT 1";

    constexpr auto kIndicatorSeries =
        "SELECT timestamp,value,parameters::text FROM technical_indicators "
        "WHERE symbol=$1 AND indicator_name=$2 AND timestamp BETWEEN $3 AND $4 "
        "ORDER BY timestamp DESC LIMIT $5";

    constexpr auto kLatestAllIndicators =
        "SELECT DISTINCT ON (indicator_name) indicator_name, value "
        "FROM technical_indicators WHERE symbol=$1 "
        "ORDER BY indicator_name, timestamp DESC";

    constexpr auto kIndicatorSnapshot =
        "SELECT DISTINCT ON (t.symbol) t.symbol, t.value "
        "FROM technical_indicators t "
        "JOIN instruments i ON i.symbol=t.symbol AND i.is_active=true "
        "WHERE t.indicator_name=$1 ORDER BY t.symbol, t.timestamp DESC";

    // ── Analytics ────────────────────────────────────────────────────────────
    constexpr auto kDailyStats =
        "SELECT symbol,bucket::text,tick_count,open_price,high_price,low_price,"
        "close_price,vwap,total_volume "
        "FROM daily_ohlcv WHERE symbol=$1 AND bucket BETWEEN $2::date AND $3::date "
        "ORDER BY bucket DESC";

    constexpr auto kRollingReturn =
        "WITH p AS (SELECT close_price FROM daily_ohlcv WHERE symbol=$1 "
        "           ORDER BY bucket DESC LIMIT $2) "
        "SELECT (FIRST_VALUE(close_price) OVER w - LAST_VALUE(close_price) OVER w) "
        "     / NULLIF(LAST_VALUE(close_price) OVER w, 0) "
        "FROM p WINDOW w AS (ORDER BY ctid ROWS BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING) "
        "LIMIT 1";

    constexpr auto kRealisedVol =
        "WITH p AS (SELECT close_price FROM daily_ohlcv WHERE symbol=$1 "
        "           ORDER BY bucket DESC LIMIT $2), "
        "lr AS (SELECT LN(close_price/LAG(close_price) OVER (ORDER BY ctid)) AS r FROM p) "
        "SELECT STDDEV(r)*SQRT(252) FROM lr WHERE r IS NOT NULL";

    constexpr auto kMostActive =
        "SELECT symbol,COUNT(*) FROM ticks WHERE timestamp BETWEEN $1 AND $2 "
        "GROUP BY symbol ORDER BY count DESC LIMIT $3";

    constexpr auto kPriceGaps =
        "WITH c AS (SELECT bucket,open_price,close_price FROM daily_ohlcv "
        "           WHERE symbol=$1 AND bucket BETWEEN $2::date AND $3::date ORDER BY bucket), "
        "g AS (SELECT bucket, LAG(close_price) OVER (ORDER BY bucket) AS prev, open_price AS op "
        "      FROM c) "
        "SELECT bucket, prev, op, (op-prev)/NULLIF(prev,0) AS pct FROM g "
        "WHERE ABS((op-prev)/NULLIF(prev,0)) > $4 ORDER BY bucket DESC";

    constexpr auto kDataFreshness =
        "SELECT (SELECT MAX(timestamp) FROM quotes WHERE symbol=$1), "
        "       (SELECT MAX(timestamp) FROM ticks  WHERE symbol=$1), "
        "       (SELECT MAX(snapshot_time) FROM order_book_snapshots WHERE symbol=$1), "
        "       (SELECT MAX(timestamp) FROM technical_indicators WHERE symbol=$1)";

    constexpr auto kCompressionStats =
        "SELECT hypertable_name, before_compression_total_bytes, "
        "after_compression_total_bytes, total_chunks, number_compressed_chunks "
        "FROM timescaledb_information.compression_settings "
        "JOIN timescaledb_information.hypertables USING (hypertable_name)";

    constexpr auto kChunkInfo =
        "SELECT hypertable_name, chunk_name, range_start, range_end, "
        "pg_total_relation_size(chunk_schema||'.'||chunk_name), is_compressed "
        "FROM timescaledb_information.chunks WHERE hypertable_name=$1 "
        "ORDER BY range_start DESC";

    constexpr auto kCompressChunks =
        "SELECT compress_chunk(c.chunk_schema||'.'||c.chunk_name) "
        "FROM timescaledb_information.chunks c "
        "WHERE c.hypertable_name=$1 AND c.range_end < $2 AND c.is_compressed=false";

    constexpr auto kRefreshAggregate =
        "CALL refresh_continuous_aggregate($1, $2, $3)";

    constexpr auto kSlowQueries =
        "SELECT query,calls,total_exec_time,mean_exec_time,rows "
        "FROM pg_stat_statements WHERE mean_exec_time > $1 "
        "ORDER BY mean_exec_time DESC LIMIT $2";
}

// =============================================================================
// § B  Tiny formatting helpers  (anonymous namespace — not part of API)
// =============================================================================
namespace {
    // Format a double to 4 decimal places as a string.
    std::string fd(double v)    { return fmt::format("{:.4f}", v); }
    // Convert an integral to string.
    template<typename T>
    std::string ts(T v)         { return std::to_string(v); }
    // Escape TSV special chars (backslash, tab, newline).
    void tsv_escape(std::string& out, std::string_view v) {
        for (char c : v) {
            if      (c == '\\') out += "\\\\";
            else if (c == '\t') out += "\\t";
            else if (c == '\n') out += "\\n";
            else                out += c;
        }
    }
    // Postgres COPY NULL marker for non-finite values.
    std::string tsv_d(double v) {
        return std::isfinite(v) ? fmt::format("{:.6f}", v) : "\\N";
    }
}

// =============================================================================
// § C  ConnectionPool
// =============================================================================
class ConnectionPool {
public:
    explicit ConnectionPool(const ConnectionConfig& cfg) : cfg_(cfg) {
        for (int i = 0; i < cfg.pool_size; ++i)
            if (auto* c = open()) { conns_.push_back(c); idle_.push(c); }
        stats_.total = idle_.size();
    }

    ~ConnectionPool() {
        std::unique_lock lk(mu_);
        while (!idle_.empty()) { PQfinish(idle_.front()); idle_.pop(); }
    }

    PGconn* acquire() {
        std::unique_lock lk(mu_);
        bool ok = cv_.wait_for(lk,
            std::chrono::milliseconds(cfg_.query_timeout_ms),
            [this]{ return !idle_.empty(); });
        if (!ok) { ++stats_.failed_acquisitions; throw std::runtime_error("pool: timeout"); }

        PGconn* c = idle_.front(); idle_.pop();
        if (PQstatus(c) != CONNECTION_OK) {
            PQreset(c);
            if (PQstatus(c) != CONNECTION_OK) {
                PQfinish(c);
                c = open();
                if (!c) { ++stats_.failed_acquisitions; throw std::runtime_error("pool: reconnect failed"); }
            }
        }
        return c;
    }

    void release(PGconn* c) noexcept {
        { std::unique_lock lk(mu_); idle_.push(c); }
        cv_.notify_one();
    }

    void refresh() {
        std::unique_lock lk(mu_);
        while (!idle_.empty()) {
            PGconn* c = idle_.front(); idle_.pop();
            PQreset(c);
            idle_.push(PQstatus(c) == CONNECTION_OK ? c : (PQfinish(c), open()));
        }
    }

    PoolStats stats() {
        std::unique_lock lk(mu_);
        stats_.idle = idle_.size();
        stats_.busy = stats_.total - idle_.size();
        return stats_;
    }

    void record(bool ok, std::chrono::microseconds dur) {
        std::unique_lock lk(mu_);
        ok ? ++stats_.queries_ok : ++stats_.queries_failed;
        // Exponential moving average (α = 0.05)
        stats_.avg_latency = std::chrono::microseconds(
            static_cast<int64_t>(stats_.avg_latency.count() * 0.95 + dur.count() * 0.05));
    }

private:
    PGconn* open() {
        auto cs = fmt::format(
            "host={} port={} dbname={} user={} password={} "
            "connect_timeout={} application_name={} sslmode={}",
            cfg_.host, cfg_.port, cfg_.dbname,
            cfg_.user, cfg_.password,
            cfg_.connect_timeout_s, cfg_.app_name, cfg_.ssl_mode);
        auto* c = PQconnectdb(cs.c_str());
        if (PQstatus(c) != CONNECTION_OK) { PQfinish(c); return nullptr; }
        auto* r = PQexec(c, fmt::format("SET statement_timeout={}", cfg_.query_timeout_ms).c_str());
        PQclear(r);
        return c;
    }

    ConnectionConfig        cfg_;
    std::vector<PGconn*>    conns_;
    std::queue<PGconn*>     idle_;
    std::mutex              mu_;
    std::condition_variable cv_;
    PoolStats               stats_{};
};

// =============================================================================
// § D  Repository  — base implementation
// =============================================================================
Repository::Repository(std::shared_ptr<ConnectionPool> pool)
    : pool_(std::move(pool)) {}

// The single non-template method that owns all pool + libpq boilerplate.
Repository::RawResult Repository::run(const std::string& sql, const Params& params) const {
    std::vector<const char*> vals;
    vals.reserve(params.size());
    for (auto& p : params) vals.push_back(p.c_str());

    auto* conn = pool_->acquire();
    auto  t0   = std::chrono::steady_clock::now();

    auto* res = PQexecParams(conn, sql.c_str(),
        static_cast<int>(vals.size()),
        nullptr, vals.data(), nullptr, nullptr, 0);

    auto dur = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0);

    ExecStatusType st = PQresultStatus(res);
    bool           ok = (st == PGRES_COMMAND_OK || st == PGRES_TUPLES_OK);
    pool_->record(ok, dur);
    pool_->release(conn);

    if (ok) return { res, {}, true };

    DbError e;
    e.message  = PQresultErrorMessage(res);
    e.sqlstate = PQresultErrorField(res, PG_DIAG_SQLSTATE) ?: "";
    e.query    = sql;
    PQclear(res);
    return { nullptr, std::move(e), false };
}

DbResult<std::size_t> Repository::copy_bulk(
    const std::string& table,
    const std::string& columns,
    std::string_view   tsv) const
{
    auto* conn = pool_->acquire();
    auto  cmd  = fmt::format(
        "COPY {} ({}) FROM STDIN WITH (FORMAT text, DELIMITER '\t', NULL '\\N')",
        table, columns);

    auto* r = PQexec(conn, cmd.c_str());
    if (PQresultStatus(r) != PGRES_COPY_IN) {
        DbError e{ PQresultErrorMessage(r), PQresultErrorField(r, PG_DIAG_SQLSTATE) ?: "", cmd };
        PQclear(r); pool_->release(conn);
        return e;
    }
    PQclear(r);

    PQputCopyData(conn, tsv.data(), static_cast<int>(tsv.size()));
    if (PQputCopyEnd(conn, nullptr) != 1) {
        pool_->release(conn);
        return DbError{"COPY end failed"};
    }

    auto* res = PQgetResult(conn);
    pool_->release(conn);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        DbError e{ PQresultErrorMessage(res) };
        PQclear(res);
        return e;
    }
    std::size_t rows = std::stoul(PQcmdTuples(res));
    PQclear(res);
    return rows;
}

// ── Column helpers ────────────────────────────────────────────────────────────
double      Repository::col_d  (PGresult* r, int row, int c) { return std::stod  (PQgetvalue(r, row, c)); }
uint64_t    Repository::col_u64(PGresult* r, int row, int c) { return std::stoull(PQgetvalue(r, row, c)); }
int         Repository::col_i  (PGresult* r, int row, int c) { return std::stoi  (PQgetvalue(r, row, c)); }
bool        Repository::col_b  (PGresult* r, int row, int c) { return PQgetvalue(r, row, c)[0] == 't'; }
std::string Repository::col_s  (PGresult* r, int row, int c) { return PQgetvalue(r, row, c); }

std::optional<TimePoint> Repository::col_ts_opt(PGresult* r, int row, int c) {
    if (PQgetisnull(r, row, c)) return {};
    return from_pg_ts(PQgetvalue(r, row, c));
}
std::optional<double> Repository::col_d_opt(PGresult* r, int row, int c) {
    if (PQgetisnull(r, row, c)) return {};
    return std::stod(PQgetvalue(r, row, c));
}
TimePoint Repository::col_ts(PGresult* r, int row, int c) {
    return from_pg_ts(PQgetvalue(r, row, c));
}

// ── Timestamp helpers ─────────────────────────────────────────────────────────
std::string Repository::to_pg_ts(TimePoint tp) {
    using namespace std::chrono;
    auto tt  = system_clock::to_time_t(tp);
    auto us  = duration_cast<microseconds>(tp.time_since_epoch()).count() % 1'000'000;
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &tt);
#else
    gmtime_r(&tt, &utc);
#endif
    return fmt::format("{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}.{:06d}+00",
        utc.tm_year+1900, utc.tm_mon+1, utc.tm_mday,
        utc.tm_hour, utc.tm_min, utc.tm_sec, static_cast<long>(us));
}

TimePoint Repository::from_pg_ts(const char* s) {
    if (!s || !*s) return TimePoint{};
    std::tm t{}; int us = 0;
    std::sscanf(s, "%d-%d-%d %d:%d:%d.%d",
        &t.tm_year, &t.tm_mon, &t.tm_mday,
        &t.tm_hour, &t.tm_min, &t.tm_sec, &us);
    t.tm_year -= 1900; t.tm_mon -= 1;
    return std::chrono::system_clock::from_time_t(std::mktime(&t))
         + std::chrono::microseconds(us);
}

std::string Repository::to_pg_interval(std::chrono::seconds s) {
    int64_t n = s.count();
    if (n <=    1) return "1 second";
    if (n <=    5) return "5 seconds";
    if (n <=   15) return "15 seconds";
    if (n <=   30) return "30 seconds";
    if (n <=   60) return "1 minute";
    if (n <=  300) return "5 minutes";
    if (n <=  900) return "15 minutes";
    if (n <= 1800) return "30 minutes";
    if (n <= 3600) return "1 hour";
    if (n <= 14400) return "4 hours";
    return "1 day";
}

// =============================================================================
// § E  InstrumentRepo
// =============================================================================
Instrument InstrumentRepo::from_row(PGresult* r, int i) {
    return { col_s(r,i,0), col_s(r,i,1), col_s(r,i,2),
             col_s(r,i,3), col_i(r,i,4), col_b(r,i,5) };
}

DbResult<bool> InstrumentRepo::upsert(const Instrument& inst) {
    return execute(sql::kUpsertInstrument, {
        inst.symbol, inst.name, inst.asset_class, inst.exchange,
        ts(inst.tick_size_decimals), inst.is_active ? "true" : "false"
    });
}

DbResult<std::size_t> InstrumentRepo::upsert_many(std::span<const Instrument> batch) {
    std::size_t count = 0;
    for (const auto& inst : batch) {
        auto r = upsert(inst);
        if (!is_ok(r)) return error_of(r);
        ++count;
    }
    return count;
}

DbResult<std::vector<Instrument>> InstrumentRepo::all(bool active_only) {
    return fetch_all<Instrument>(
        active_only ? sql::kSelectInstrumentsActive : sql::kSelectInstruments,
        {}, from_row);
}

DbResult<std::optional<Instrument>> InstrumentRepo::find(const Symbol& sym) {
    return fetch_one<Instrument>(sql::kFindInstrument, {sym}, from_row);
}

DbResult<bool> InstrumentRepo::set_active(const Symbol& sym, bool active) {
    return execute(sql::kSetInstrumentActive, {sym, active ? "true" : "false"});
}

// =============================================================================
// § F  QuoteRepo
// =============================================================================
Quote QuoteRepo::from_row(PGresult* r, int i) {
    Quote q;
    q.symbol     = col_s(r,i,0);
    q.price      = col_d(r,i,1);
    q.open       = col_d(r,i,2);
    q.high       = col_d(r,i,3);
    q.low        = col_d(r,i,4);
    q.volume     = col_u64(r,i,5);
    q.change_pct = col_d(r,i,6);
    q.source     = col_s(r,i,7);
    q.timestamp  = col_ts(r,i,8);
    return q;
}

std::string QuoteRepo::to_tsv(const Symbol& sym, const Quote& q) {
    std::string row;
    tsv_escape(row, sym);         row += '\t';
    row += tsv_d(q.price);        row += '\t';
    row += tsv_d(q.open);         row += '\t';
    row += tsv_d(q.high);         row += '\t';
    row += tsv_d(q.low);          row += '\t';
    row += ts(q.volume);          row += '\t';
    row += tsv_d(q.change_pct);   row += '\t';
    tsv_escape(row, q.source);    row += '\t';
    row += to_pg_ts(q.timestamp); row += '\n';
    return row;
}

DbResult<bool> QuoteRepo::store(const Symbol& sym, const Quote& q) {
    return execute(sql::kInsertQuote, {
        sym, fd(q.price), fd(q.open), fd(q.high), fd(q.low),
        ts(q.volume), fd(q.change_pct), q.source, to_pg_ts(q.timestamp)
    });
}

DbResult<std::size_t> QuoteRepo::store_many(std::span<const Quote> quotes) {
    if (quotes.empty()) return std::size_t{0};
    std::string tsv;
    tsv.reserve(quotes.size() * 100);
    for (const auto& q : quotes) tsv += to_tsv(q.symbol, q);
    return copy_bulk("quotes",
        "symbol,price,open,high,low,volume,change_pct,source,timestamp", tsv);
}

DbResult<std::vector<Quote>> QuoteRepo::range(
    const Symbol& sym, TimePoint from, TimePoint to, std::size_t limit)
{
    return fetch_all<Quote>(sql::kSelectQuotes,
        { sym, to_pg_ts(from), to_pg_ts(to), ts(limit) }, from_row);
}

DbResult<std::optional<Quote>> QuoteRepo::latest(const Symbol& sym) {
    return fetch_one<Quote>(sql::kSelectQuotes,
        { sym, to_pg_ts(TimePoint{}), to_pg_ts(std::chrono::system_clock::now()), "1" },
        from_row);
}

DbResult<double> QuoteRepo::latest_price(const Symbol& sym) {
    return fetch_scalar<double>(sql::kLatestPrice, {sym});
}

DbResult<std::unordered_map<Symbol, double>>
QuoteRepo::latest_prices(std::span<const Symbol> syms) {
    if (syms.empty()) return std::unordered_map<Symbol,double>{};
    // Build  '{AAPL,MSFT,GOOGL}'  as an array literal for ANY($1).
    std::string arr = "{";
    for (std::size_t i = 0; i < syms.size(); ++i) { if (i) arr += ','; arr += syms[i]; }
    arr += "}";
    auto rows = fetch_all<std::pair<Symbol,double>>(sql::kLatestPrices, {arr},
        [](PGresult* r, int i) {
            return std::pair<Symbol,double>{ PQgetvalue(r,i,0), std::stod(PQgetvalue(r,i,1)) };
        });
    if (!is_ok(rows)) return error_of(rows);
    std::unordered_map<Symbol,double> out;
    for (auto& p : unwrap(rows)) out.insert(std::move(p));
    return out;
}

// =============================================================================
// § G  TickRepo
// =============================================================================
Tick TickRepo::from_row(PGresult* r, int i) {
    Tick t;
    t.symbol    = col_s(r,i,0);
    t.price     = col_d(r,i,1);
    t.size      = col_u64(r,i,2);
    t.side      = col_s(r,i,3)[0];
    t.source    = col_s(r,i,4);
    t.timestamp = col_ts(r,i,5);
    return t;
}

Candle TickRepo::candle_row(PGresult* r, int i) {
    return { col_s(r,i,0), col_ts(r,i,1),
             col_d(r,i,2), col_d(r,i,3), col_d(r,i,4), col_d(r,i,5),
             col_u64(r,i,6), static_cast<uint32_t>(col_u64(r,i,7)) };
}

std::string TickRepo::to_tsv(const Tick& t) {
    std::string row;
    tsv_escape(row, t.symbol);      row += '\t';
    row += tsv_d(t.price);          row += '\t';
    row += ts(t.size);              row += '\t';
    row += t.side;                  row += '\t';
    tsv_escape(row, t.source);      row += '\t';
    row += to_pg_ts(t.timestamp);   row += '\n';
    return row;
}

DbResult<bool> TickRepo::store(const Tick& t) {
    return execute(sql::kInsertTick, {
        t.symbol, fd(t.price), ts(t.size), std::string(1, t.side),
        t.source, to_pg_ts(t.timestamp)
    });
}

DbResult<std::size_t> TickRepo::store_many(std::span<const Tick> ticks) {
    if (ticks.empty()) return std::size_t{0};
    std::string tsv;
    tsv.reserve(ticks.size() * 80);
    for (const auto& t : ticks) tsv += to_tsv(t);
    return copy_bulk("ticks", "symbol,price,size,side,source,timestamp", tsv);
}

DbResult<std::vector<Tick>> TickRepo::range(
    const Symbol& sym, TimePoint from, TimePoint to, std::size_t limit)
{
    return fetch_all<Tick>(sql::kSelectTicks,
        { sym, to_pg_ts(from), to_pg_ts(to), ts(limit) }, from_row);
}

DbResult<uint64_t> TickRepo::count(const Symbol& sym, TimePoint from, TimePoint to) {
    return fetch_scalar<uint64_t>(sql::kCountTicks, { sym, to_pg_ts(from), to_pg_ts(to) });
}

DbResult<double> TickRepo::vwap(const Symbol& sym, TimePoint from, TimePoint to) {
    return fetch_scalar<double>(sql::kVwap, { sym, to_pg_ts(from), to_pg_ts(to) });
}

DbResult<std::vector<Candle>> TickRepo::candles(
    const Symbol& sym, std::chrono::seconds interval,
    TimePoint from, TimePoint to, std::size_t limit)
{
    return fetch_all<Candle>(sql::kCandles,
        { to_pg_interval(interval), sym, to_pg_ts(from), to_pg_ts(to), ts(limit) },
        candle_row);
}

DbResult<std::vector<Candle>> TickRepo::daily_ohlcv(
    const Symbol& sym, const std::string& from_date,
    const std::string& to_date, std::size_t limit)
{
    return fetch_all<Candle>(sql::kDailyOhlcv,
        { sym, from_date, to_date, ts(limit) }, candle_row);
}

DbResult<TimePoint> TickRepo::stream(
    const Symbol& sym, TimePoint since, OnTick handler, std::size_t batch)
{
    auto rows = fetch_all<Tick>(sql::kStreamTicks, { sym, to_pg_ts(since), ts(batch) }, from_row);
    if (!is_ok(rows)) return error_of(rows);
    TimePoint last = since;
    for (auto& t : unwrap(rows)) { last = t.timestamp; handler(t); }
    return last;
}

// =============================================================================
// § H  OrderBookRepo
// =============================================================================
OrderBookSnapshot OrderBookRepo::snapshot_row(PGresult* r, int i) {
    OrderBookSnapshot s;
    s.symbol        = col_s(r,i,0);
    s.best_bid      = col_d(r,i,1);
    s.best_ask      = col_d(r,i,2);
    s.mid_price     = col_d(r,i,3);
    s.spread        = col_d(r,i,4);
    s.imbalance     = col_d(r,i,5);
    s.total_bid_vol = col_u64(r,i,6);
    s.total_ask_vol = col_u64(r,i,7);
    s.snapshot_time = col_ts(r,i,8);
    return s;
}

HourlyBookRow OrderBookRepo::hourly_row(PGresult* r, int i) {
    return { col_ts(r,i,0), col_d(r,i,1), col_d(r,i,2),
             col_d(r,i,3), col_d(r,i,4), col_d(r,i,5) };
}

DbResult<bool> OrderBookRepo::store(
    const Symbol& sym, double best_bid, double best_ask,
    double imbalance, uint64_t bid_vol, uint64_t ask_vol)
{
    return execute(sql::kInsertSnapshot, {
        sym, fd(best_bid), fd(best_ask), fd(imbalance), ts(bid_vol), ts(ask_vol)
    });
}

DbResult<std::optional<OrderBookSnapshot>> OrderBookRepo::latest(const Symbol& sym) {
    return fetch_one<OrderBookSnapshot>(sql::kLatestSnapshot, {sym}, snapshot_row);
}

DbResult<SpreadStats> OrderBookRepo::spread_stats(
    const Symbol& sym, TimePoint from, TimePoint to)
{
    auto q = run(sql::kSpreadStats, { sym, to_pg_ts(from), to_pg_ts(to) });
    if (!q.ok) return q.err;
    SpreadStats s;
    s.symbol        = sym;
    s.avg_spread    = col_d_opt(q.res,0,0).value_or(0.0);
    s.min_spread    = col_d_opt(q.res,0,1).value_or(0.0);
    s.max_spread    = col_d_opt(q.res,0,2).value_or(0.0);
    s.avg_imbalance = col_d_opt(q.res,0,3).value_or(0.0);
    s.avg_mid       = col_d_opt(q.res,0,4).value_or(0.0);
    s.sample_count  = col_u64(q.res,0,5);
    return s;
}

DbResult<std::vector<HourlyBookRow>> OrderBookRepo::hourly(
    const Symbol& sym, const std::string& from_date, const std::string& to_date)
{
    return fetch_all<HourlyBookRow>(sql::kHourlySummary,
        { sym, from_date, to_date }, hourly_row);
}

// =============================================================================
// § I  IndicatorRepo
// =============================================================================
IndicatorPoint IndicatorRepo::from_row(PGresult* r, int i) {
    return { col_ts(r,i,0), col_d(r,i,1), col_s(r,i,2) };
}

std::string IndicatorRepo::to_tsv(
    const Symbol& sym, const std::string& name, const IndicatorPoint& pt)
{
    std::string row;
    tsv_escape(row, sym);                             row += '\t';
    tsv_escape(row, name);                            row += '\t';
    row += tsv_d(pt.value);                           row += '\t';
    tsv_escape(row, pt.parameters.empty() ? "{}" : pt.parameters); row += '\t';
    row += to_pg_ts(pt.timestamp);                    row += '\n';
    return row;
}

DbResult<bool> IndicatorRepo::store(
    const Symbol& sym, const std::string& name,
    double value, const std::string& params_json)
{
    return execute(sql::kInsertIndicator,
        { sym, name, fd(value), params_json.empty() ? "{}" : params_json });
}

DbResult<std::size_t> IndicatorRepo::store_many(
    const Symbol& sym, const std::string& name,
    std::span<const IndicatorPoint> points)
{
    if (points.empty()) return std::size_t{0};
    std::string tsv;
    tsv.reserve(points.size() * 90);
    for (const auto& pt : points) tsv += to_tsv(sym, name, pt);
    return copy_bulk("technical_indicators",
        "symbol,indicator_name,value,parameters,timestamp", tsv);
}

DbResult<std::optional<double>> IndicatorRepo::last_value(
    const Symbol& sym, const std::string& name)
{
    auto r = fetch_scalar<double>(sql::kLastIndicator, { sym, name });
    if (!is_ok(r)) return error_of(r);
    return std::optional<double>(unwrap(r));
}

DbResult<std::vector<IndicatorPoint>> IndicatorRepo::series(
    const Symbol& sym, const std::string& name,
    TimePoint from, TimePoint to, std::size_t limit)
{
    return fetch_all<IndicatorPoint>(sql::kIndicatorSeries,
        { sym, name, to_pg_ts(from), to_pg_ts(to), ts(limit) }, from_row);
}

DbResult<std::unordered_map<std::string, double>>
IndicatorRepo::latest_all(const Symbol& sym)
{
    auto rows = fetch_all<std::pair<std::string,double>>(
        sql::kLatestAllIndicators, {sym},
        [](PGresult* r, int i) {
            return std::pair<std::string,double>{ PQgetvalue(r,i,0), std::stod(PQgetvalue(r,i,1)) };
        });
    if (!is_ok(rows)) return error_of(rows);
    std::unordered_map<std::string,double> out;
    for (auto& p : unwrap(rows)) out.insert(std::move(p));
    return out;
}

DbResult<std::unordered_map<Symbol, double>>
IndicatorRepo::snapshot(const std::string& name)
{
    auto rows = fetch_all<std::pair<Symbol,double>>(
        sql::kIndicatorSnapshot, {name},
        [](PGresult* r, int i) {
            return std::pair<Symbol,double>{ PQgetvalue(r,i,0), std::stod(PQgetvalue(r,i,1)) };
        });
    if (!is_ok(rows)) return error_of(rows);
    std::unordered_map<Symbol,double> out;
    for (auto& p : unwrap(rows)) out.insert(std::move(p));
    return out;
}

// =============================================================================
// § J  AnalyticsRepo
// =============================================================================
DailyStats AnalyticsRepo::daily_row(PGresult* r, int i) {
    return { col_s(r,i,0), col_s(r,i,1), col_u64(r,i,2),
             col_d(r,i,3), col_d(r,i,4), col_d(r,i,5), col_d(r,i,6),
             col_d_opt(r,i,7).value_or(0.0), col_u64(r,i,8) };
}

PriceGap AnalyticsRepo::gap_row(PGresult* r, int i) {
    return { col_ts(r,i,0), col_d(r,i,1), col_d(r,i,2), col_d(r,i,3)*100.0 };
}

ChunkInfo AnalyticsRepo::chunk_row(PGresult* r, int i) {
    return { col_s(r,i,0), col_s(r,i,1), col_ts(r,i,2), col_ts(r,i,3),
             col_u64(r,i,4), col_b(r,i,5) };
}

QueryStat AnalyticsRepo::stat_row(PGresult* r, int i) {
    return { col_s(r,i,0), col_u64(r,i,1), col_u64(r,i,4),
             col_d(r,i,2), col_d(r,i,3) };
}

DbResult<std::vector<DailyStats>> AnalyticsRepo::daily_stats(
    const Symbol& sym, const std::string& from, const std::string& to)
{
    return fetch_all<DailyStats>(sql::kDailyStats, { sym, from, to }, daily_row);
}

DbResult<double> AnalyticsRepo::rolling_return(const Symbol& sym, int days) {
    return fetch_scalar<double>(sql::kRollingReturn, { sym, ts(days) });
}

DbResult<double> AnalyticsRepo::realised_vol(const Symbol& sym, int days) {
    return fetch_scalar<double>(sql::kRealisedVol, { sym, ts(days + 1) });
}

DbResult<std::vector<std::pair<Symbol, uint64_t>>>
AnalyticsRepo::most_active(TimePoint from, TimePoint to, std::size_t top_n)
{
    return fetch_all<std::pair<Symbol,uint64_t>>(sql::kMostActive,
        { to_pg_ts(from), to_pg_ts(to), ts(top_n) },
        [](PGresult* r, int i) {
            return std::pair<Symbol,uint64_t>{ PQgetvalue(r,i,0), std::stoull(PQgetvalue(r,i,1)) };
        });
}

DbResult<std::vector<PriceGap>> AnalyticsRepo::price_gaps(
    const Symbol& sym, double threshold_pct, TimePoint from, TimePoint to)
{
    return fetch_all<PriceGap>(sql::kPriceGaps,
        { sym, to_pg_ts(from), to_pg_ts(to), fmt::format("{:.6f}", threshold_pct/100.0) },
        gap_row);
}

DbResult<DataFreshness> AnalyticsRepo::freshness(const Symbol& sym) {
    auto q = run(sql::kDataFreshness, {sym});
    if (!q.ok) return q.err;
    return DataFreshness{ sym,
        col_ts_opt(q.res,0,0), col_ts_opt(q.res,0,1),
        col_ts_opt(q.res,0,2), col_ts_opt(q.res,0,3) };
}

DbResult<std::vector<CompressionStats>> AnalyticsRepo::compression() {
    return fetch_all<CompressionStats>(sql::kCompressionStats, {},
        [](PGresult* r, int i) {
            CompressionStats cs;
            cs.hypertable          = PQgetvalue(r,i,0);
            cs.uncompressed_bytes  = std::stoul(PQgetvalue(r,i,1));
            cs.compressed_bytes    = std::stoul(PQgetvalue(r,i,2));
            cs.ratio               = cs.compressed_bytes > 0
                                     ? static_cast<double>(cs.uncompressed_bytes)
                                       / cs.compressed_bytes : 0.0;
            cs.total_chunks        = std::stoul(PQgetvalue(r,i,3));
            cs.compressed_chunks   = std::stoul(PQgetvalue(r,i,4));
            return cs;
        });
}

DbResult<std::vector<ChunkInfo>> AnalyticsRepo::chunks(const std::string& hypertable) {
    return fetch_all<ChunkInfo>(sql::kChunkInfo, {hypertable}, chunk_row);
}

DbResult<uint32_t> AnalyticsRepo::compress_old(
    const std::string& hypertable, std::chrono::system_clock::duration older_than)
{
    auto cutoff = std::chrono::system_clock::now() - older_than;
    auto q = run(sql::kCompressChunks, { hypertable, to_pg_ts(cutoff) });
    if (!q.ok) return q.err;
    return static_cast<uint32_t>(PQntuples(q.res));
}

DbResult<bool> AnalyticsRepo::refresh(
    const std::string& view, TimePoint from, TimePoint to)
{
    return execute(sql::kRefreshAggregate, { view, to_pg_ts(from), to_pg_ts(to) });
}

DbResult<bool> AnalyticsRepo::refresh_all() {
    static constexpr std::array views = { "daily_ohlcv", "hourly_book_summary" };
    auto now   = std::chrono::system_clock::now();
    auto start = now - std::chrono::hours(24 * 7);
    for (const char* v : views) {
        auto r = refresh(v, start, now);
        if (!is_ok(r)) return r;
    }
    return true;
}

DbResult<std::vector<QueryStat>> AnalyticsRepo::slow_queries(
    double min_mean_ms, std::size_t limit)
{
    return fetch_all<QueryStat>(sql::kSlowQueries,
        { fmt::format("{:.3f}", min_mean_ms), ts(limit) }, stat_row);
}

// =============================================================================
// § K  Transaction
// =============================================================================
class TransactionImpl : public Transaction {
public:
    TransactionImpl(PGconn* conn, std::shared_ptr<ConnectionPool> pool)
        : conn_(conn), pool_(std::move(pool))
    {
        auto* r = PQexec(conn_, "BEGIN");
        if (PQresultStatus(r) != PGRES_COMMAND_OK) {
            std::string msg = PQresultErrorMessage(r);
            PQclear(r); pool_->release(conn_); conn_ = nullptr;
            throw std::runtime_error("BEGIN failed: " + msg);
        }
        PQclear(r);
    }

    ~TransactionImpl() override {
        if (conn_) { auto* r = PQexec(conn_, "ROLLBACK"); PQclear(r); pool_->release(conn_); }
    }

    DbResult<bool> commit() override {
        if (!conn_) return DbError{"not active"};
        auto* r = PQexec(conn_, "COMMIT");
        bool ok = PQresultStatus(r) == PGRES_COMMAND_OK;
        DbError e{ PQresultErrorMessage(r) };
        PQclear(r);
        pool_->release(conn_); conn_ = nullptr;
        return ok ? DbResult<bool>{true} : DbResult<bool>{std::move(e)};
    }

    DbResult<bool> rollback() override {
        if (!conn_) return DbError{"not active"};
        auto* r = PQexec(conn_, "ROLLBACK");
        PQclear(r); pool_->release(conn_); conn_ = nullptr;
        return true;
    }

    [[nodiscard]] bool is_active() const noexcept override { return conn_ != nullptr; }

private:
    PGconn*                         conn_;
    std::shared_ptr<ConnectionPool> pool_;
};

// =============================================================================
// § L  PostgresClient — Facade
// =============================================================================
PostgresClient::PostgresClient(ConnectionConfig cfg) {
    pool_        = std::make_shared<ConnectionPool>(cfg);
    instruments_ = std::make_unique<InstrumentRepo>(pool_);
    quotes_      = std::make_unique<QuoteRepo>(pool_);
    ticks_       = std::make_unique<TickRepo>(pool_);
    order_book_  = std::make_unique<OrderBookRepo>(pool_);
    indicators_  = std::make_unique<IndicatorRepo>(pool_);
    analytics_   = std::make_unique<AnalyticsRepo>(pool_);
}

PostgresClient::~PostgresClient() = default;

bool PostgresClient::ping() const {
    auto* conn = pool_->acquire();
    auto* r    = PQexec(conn, "SELECT 1");
    bool ok    = PQresultStatus(r) == PGRES_TUPLES_OK;
    PQclear(r); pool_->release(conn);
    return ok;
}

PoolStats PostgresClient::pool_stats() const { return pool_->stats(); }
void      PostgresClient::reconnect()        { pool_->refresh(); }

std::unique_ptr<Transaction> PostgresClient::begin_transaction() {
    return std::make_unique<TransactionImpl>(pool_->acquire(), pool_);
}

DbResult<bool> PostgresClient::transaction(std::function<void()> fn) {
    auto tx = begin_transaction();
    try { fn(); return tx->commit(); }
    catch (const std::exception& ex) {
        tx->rollback();
        return DbError{ std::string("transaction: ") + ex.what() };
    }
}

void PostgresClient::prepare_statements() {
    // Prepare hot-path INSERT statements on all pool connections.
    // For brevity a single PQprepare call is shown; extend per-stmt as needed.
    auto* conn = pool_->acquire();
    static constexpr std::array stmts = {
        std::pair{ "ins_tick",  sql::kInsertTick  },
        std::pair{ "ins_quote", sql::kInsertQuote },
        std::pair{ "ins_ind",   sql::kInsertIndicator },
        std::pair{ "ins_snap",  sql::kInsertSnapshot  },
    };
    for (auto& [name, query] : stmts) {
        auto* r = PQprepare(conn, name, query, 0, nullptr);
        PQclear(r);
    }
    pool_->release(conn);
}

}  // namespace fincore::db


