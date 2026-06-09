// This module provides a robust, production‑ready interface to Redis Streams
// using the hiredis C library.It follows several well‑established idioms:
//
//   * Builder   - StreamConfig::builder() allows readable construction of
//                 connection and stream parameters.
//   * Strategy  - Callbacks for error handling and deserialisation allow the
//                 consumer to plug in application‑specific logic.
//   * RAII      - RedisConnection manages the hiredis context lifecycle.
//   Key:     "ticks:{SYMBOL}"
//   Fields:  price, size, side, source, ts_us
//
//   Key:     "quotes:{SYMBOL}"
//   Fields:  price, open, high, low, volume, change_pct, source, ts_us
#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/types.hpp"

// Forward declarations - keep hiredis types out of client translation units.
struct redisContext;
struct redisReply;

namespace fincore::feed {

// =============================================================================
// StreamConfig - connection and behaviour parameters.
//
// Constructed via the fluent builder returned by StreamConfig::builder().
// Example:
//   auto cfg = StreamConfig::builder()
//                  .host("redis.prod")
//                  .port(6380)
//                  .password("secret")
//                  .max_stream_len(100'000)
//                  .build();
// =============================================================================
struct StreamConfig {
    std::string host{"localhost"};
    int         port{6379};
    std::string password;               // if empty, no AUTH is performed
    int         db_index{0};            // Redis logical database index (0-15)
    int         connect_timeout_ms{3000};
    int         command_timeout_ms{1000};
    int         reconnect_delay_ms{500};
    int         max_reconnect_attempts{10};

    // XADD behaviour
    std::size_t max_stream_len{500000};    // MAXLEN ~ approximate trimming
    bool        approx_trimming{true};      // use '~' for O(1) trimming

    // Consumer group configuration
    std::string group_name{"fincore-drainer"};
    std::string consumer_name;              // defaults to hostname+PID in .cpp

    // XREADGROUP batch size
    std::size_t read_batch_size{200};

    // Block duration for XREAD / XREADGROUP (0 = non‑blocking poll)
    std::chrono::milliseconds block_ms{100};

    // -------------------------------------------------------------------------
    // Fluent builder for StreamConfig.
    // -------------------------------------------------------------------------
    class Builder {
    public:
        Builder& host              (std::string v)   { c_.host              = std::move(v); return *this; }
        Builder& port              (int v)             { c_.port              = v;            return *this; }
        Builder& password          (std::string v)   { c_.password          = std::move(v); return *this; }
        Builder& db_index          (int v)             { c_.db_index          = v;            return *this; }
        Builder& connect_timeout_ms(int v)             { c_.connect_timeout_ms= v;            return *this; }
        Builder& command_timeout_ms(int v)             { c_.command_timeout_ms= v;            return *this; }
        Builder& max_stream_len    (std::size_t v)    { c_.max_stream_len    = v;            return *this; }
        Builder& approx_trimming   (bool v)            { c_.approx_trimming   = v;            return *this; }
        Builder& group_name        (std::string v)   { c_.group_name        = std::move(v); return *this; }
        Builder& consumer_name     (std::string v)   { c_.consumer_name     = std::move(v); return *this; }
        Builder& read_batch_size   (std::size_t v)    { c_.read_batch_size   = v;            return *this; }
        Builder& block_ms          (int v)             { c_.block_ms          = std::chrono::milliseconds(v); return *this; }
        StreamConfig build() { return std::move(c_); }
    private:
        StreamConfig c_;
    };

    static Builder builder() { return {}; }
};

// =============================================================================
// StreamId - Redis stream entry identifier.
//
// Format: "<milliseconds>-<sequence>"
// -----------------------------------------------------------------------------
struct StreamId {
    uint64_t ms{};
    uint64_t seq{};

    static StreamId zero() { return {0, 0}; }
    static StreamId last() { return {UINT64_MAX, UINT64_MAX}; }   // corresponds to "$"

    // Parse from a string_view; throws on invalid input.
    static StreamId parse(std::string_view s);

    // Convert to Redis wire format.
    std::string str() const;

    bool operator< (const StreamId& o) const noexcept { return ms < o.ms || (ms == o.ms && seq < o.seq); }
    bool operator==(const StreamId& o) const noexcept { return ms == o.ms && seq == o.seq; }
};

// =============================================================================
// StreamEntry - raw, uninterpreted message from a stream.
// -----------------------------------------------------------------------------
struct StreamEntry {
    StreamId                                   id;
    std::unordered_map<std::string, std::string> fields;
};

// =============================================================================
// RedisConnection - RAII wrapper around a hiredis redisContext*.
//
// Internal to the stream classes; exposed for testing and extension.
// -----------------------------------------------------------------------------
class RedisConnection {
public:
    explicit RedisConnection(const StreamConfig& cfg);
    ~RedisConnection();

    RedisConnection(const RedisConnection&)            = delete;
    RedisConnection& operator=(const RedisConnection&) = delete;
    RedisConnection(RedisConnection&&)                 = default;

    [[nodiscard]] bool is_connected() const noexcept;
    bool reconnect();

    // Execute a formatted command.  Returns nullptr on network error.
    // The caller owns the returned redisReply* and must free it with
    // freeReplyObject().
    redisReply* command(const char* fmt, ...);

    // Execute a command using pre‑built argv / argvlen arrays.
    redisReply* command_argv(int argc, const char** argv, const std::size_t* argvlen);

    const StreamConfig& config() const noexcept { return cfg_; }

private:
    StreamConfig   cfg_;
    redisContext*  ctx_{nullptr};

    void connect();
    void set_timeout();
    void auth();
    void select_db();
};

// =============================================================================
// TickPublisher - writes ticks and quotes into Redis Streams via XADD.
//
// Example:
//   TickPublisher pub(cfg);
//   pub.publish_tick(tick);
//   pub.publish_quote("AAPL", quote);
//   pub.publish_batch(ticks);   // single round‑trip for the whole batch
// -----------------------------------------------------------------------------
class TickPublisher {
public:
    explicit TickPublisher(StreamConfig cfg);

    // Single‑entry writes.
    std::optional<StreamId> publish_tick (const Tick& tick);
    std::optional<StreamId> publish_quote(const Symbol& sym, const Quote& quote);

    // Pipelined batch writes - one network round‑trip per batch.
    std::size_t publish_batch(const std::vector<Tick>& ticks);
    std::size_t publish_batch(const std::vector<std::pair<Symbol, Quote>>& quotes);

    // Connection liveness check.
    [[nodiscard]] bool ping();

    // Stream key naming conventions.
    static std::string tick_key (const Symbol& sym) { return "ticks:"  + sym; }
    static std::string quote_key(const Symbol& sym) { return "quotes:" + sym; }

private:
    RedisConnection conn_;
    StreamConfig    cfg_;

    std::optional<StreamId> xadd(const std::string& key,
                                 const std::vector<std::string>& fields);
};

// =============================================================================
// TickConsumer - reads from Redis Streams using consumer groups (XREADGROUP).
//
// Typical usage:
//   TickConsumer consumer(cfg);
//   consumer.ensure_groups({"AAPL", "MSFT", "BTC"});
//
//   consumer.on_tick ([](const Tick& t)          { book.apply_tick(t); });
//   consumer.on_quote([](const Symbol& s, const Quote& q) { db.quotes().store(s, q); });
//
//   while (running) {
//       auto n = consumer.poll();    // blocks up to block_ms, returns count
//       consumer.ack_pending();      // acknowledge processed messages
//   }
// -----------------------------------------------------------------------------
class TickConsumer {
public:
    using TickHandler  = std::function<void(const Tick&)>;
    using QuoteHandler = std::function<void(const Symbol&, const Quote&)>;
    using ErrorHandler = std::function<void(std::string_view msg)>;

    explicit TickConsumer(StreamConfig cfg);

    // -------------------------------------------------------------------------
    // Group setup - idempotent (XGROUP CREATE ... MKSTREAM $).
    // -------------------------------------------------------------------------
    bool ensure_groups(const std::vector<Symbol>& symbols);
    bool ensure_group (const Symbol& sym);

    // -------------------------------------------------------------------------
    // Callback registration.
    // -------------------------------------------------------------------------
    void on_tick (TickHandler  h) { tick_handler_  = std::move(h); }
    void on_quote(QuoteHandler h) { quote_handler_ = std::move(h); }
    void on_error(ErrorHandler h) { error_handler_ = std::move(h); }

    // -------------------------------------------------------------------------
    // Polling - reads up to cfg.read_batch_size messages from all subscribed
    // streams, deserialises, and invokes the appropriate handlers.
    // Returns the total number of entries dispatched.
    // -------------------------------------------------------------------------
    std::size_t poll();

    // Re‑deliver messages from the PEL (pending entries list) that have been
    // idle for at least `min_idle`.  Call this after a restart to recover
    // unacknowledged work.
    std::size_t recover_pending(const Symbol& sym,
                                std::chrono::milliseconds min_idle = std::chrono::minutes(1));

    // Acknowledge all entries processed in the most recent poll() call.
    void ack_pending();

    // -------------------------------------------------------------------------
    // Drainer loop - runs poll() → ack_pending() until stop() is called.
    // Intended to be run on a dedicated thread.
    // -------------------------------------------------------------------------
    void run();
    void stop() noexcept { running_ = false; }

    [[nodiscard]] bool is_running() const noexcept { return running_; }

    // Statistics collected during operation.
    struct Stats {
        uint64_t ticks_dispatched{};
        uint64_t quotes_dispatched{};
        uint64_t parse_errors{};
        uint64_t reconnects{};
        uint64_t acks_sent{};
    };
    [[nodiscard]] Stats stats() const noexcept { return stats_; }

private:
    StreamConfig    cfg_;
    RedisConnection conn_;
    bool            running_{false};

    std::vector<Symbol>   subscribed_;            // symbols with consumer groups
    std::vector<StreamId> last_ids_;              // parallel to subscribed_
    std::vector<StreamId> pending_ack_ids_;       // entries to be acknowledged
    std::vector<Symbol>   pending_ack_streams_;   // parallel to pending_ack_ids_

    TickHandler  tick_handler_;
    QuoteHandler quote_handler_;
    ErrorHandler error_handler_;
    Stats        stats_{};

    // Deserialisation helpers.
    std::optional<Tick>  parse_tick (const Symbol& sym, const StreamEntry& e);
    std::optional<Quote> parse_quote(const Symbol& sym, const StreamEntry& e);

    // Parse an XREADGROUP reply into a map of stream → vector<StreamEntry>.
    using StreamEntries = std::unordered_map<Symbol, std::vector<StreamEntry>>;
    StreamEntries parse_xreadgroup_reply(redisReply* reply);

    void emit_error(std::string_view msg);
};

}  // namespace fincore::feed
