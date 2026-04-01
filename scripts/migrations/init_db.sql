\c fincore;

CREATE EXTENSION IF NOT EXISTS timescaledb;
CREATE EXTENSION IF NOT EXISTS pg_stat_statements;


-- SCHEMA VERSIONING
-- Lets track migrations and reason about deploys.
CREATE TABLE schema_versions (
    version     VARCHAR(20) PRIMARY KEY,
    description TEXT,
    applied_at  TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
INSERT INTO schema_versions VALUES ('1.0.0', 'Initial fincore schema', NOW());


-- REFERENCE TABLES
-- Centralise symbol and source metadata so every time-series table can FK
-- into them instead of duplicating varchar data across millions of rows.
CREATE TABLE instruments (
    symbol              VARCHAR(10)  PRIMARY KEY,
    name                VARCHAR(100) NOT NULL,
    asset_class         VARCHAR(20)  NOT NULL DEFAULT 'EQUITY'
                            CHECK (asset_class IN ('EQUITY','FOREX','CRYPTO','FUTURES','ETF')),
    exchange            VARCHAR(20),
    tick_size_decimals  SMALLINT     NOT NULL DEFAULT 4,
    is_active           BOOLEAN      NOT NULL DEFAULT TRUE,
    created_at          TIMESTAMPTZ  NOT NULL DEFAULT NOW(),
    updated_at          TIMESTAMPTZ  NOT NULL DEFAULT NOW()
);
COMMENT ON TABLE instruments IS 'Master list of tradeable instruments. FK target for all time-series tables.';


-- Data to get info for tests
CREATE TABLE data_sources (
    code         VARCHAR(20)  PRIMARY KEY,
    display_name VARCHAR(60)  NOT NULL,
    base_url     TEXT,
    is_active    BOOLEAN      NOT NULL DEFAULT TRUE
);
INSERT INTO data_sources (code, display_name) VALUES
    ('ALPHA_VANTAGE', 'Alpha Vantage');


-- HYPERTABLE: quotes
-- OHLCV data. Partition by day — suits 1-min to 1-day bar queries.
CREATE TABLE quotes (
    -- No BIGSERIAL id. TimescaleDB chunks by timestamp; a global sequence
    -- is wasted overhead and a write bottleneck at scale.
    symbol          VARCHAR(10)    NOT NULL REFERENCES instruments(symbol),
    price           NUMERIC(14,4)  NOT NULL CHECK (price > 0),
    open            NUMERIC(14,4)           CHECK (open > 0),
    high            NUMERIC(14,4)           CHECK (high > 0),
    low             NUMERIC(14,4)           CHECK (low > 0),
    volume          BIGINT                  CHECK (volume >= 0),
    change_pct      NUMERIC(10,4),          -- nullable; not always available
    source          VARCHAR(20)    NOT NULL DEFAULT 'ALPHA_VANTAGE'
                        REFERENCES data_sources(code),
    timestamp       TIMESTAMPTZ    NOT NULL DEFAULT NOW(),

    -- OHLC sanity
    CONSTRAINT chk_quotes_hl CHECK (high IS NULL OR low IS NULL OR high >= low)
);

SELECT create_hypertable('quotes', 'timestamp',
    chunk_time_interval => INTERVAL '1 day');

-- Covering index for the most common access pattern: latest N bars for a symbol
CREATE INDEX idx_quotes_symbol_time
    ON quotes (symbol, timestamp DESC)
    INCLUDE (price, volume);

-- For cross-symbol scans (e.g. screeners)
CREATE INDEX idx_quotes_time
    ON quotes (timestamp DESC);

-- TimescaleDB native compression — compresses chunks older than 7 days.
-- Typical ratio: 10–20x. MUST be set before adding retention policy.
ALTER TABLE quotes SET (
    timescaledb.compress,
    timescaledb.compress_orderby     = 'timestamp DESC',
    timescaledb.compress_segmentby   = 'symbol'
);
SELECT add_compression_policy('quotes', INTERVAL '7 days');

-- Retain 2 years of quote data
SELECT add_retention_policy('quotes', INTERVAL '2 years');

-- High-frequency trade prints. Partition by 1 hour.
CREATE TABLE ticks (
    symbol      VARCHAR(10)   NOT NULL REFERENCES instruments(symbol),
    price       NUMERIC(14,4) NOT NULL CHECK (price > 0),
    size        BIGINT        NOT NULL CHECK (size > 0),
    side        CHAR(1)       NOT NULL CHECK (side IN ('B', 'A', 'U')), -- Bid/Ask/Unknown
    source      VARCHAR(20)   NOT NULL DEFAULT 'STREAM'
                    REFERENCES data_sources(code),
    timestamp   TIMESTAMPTZ   NOT NULL DEFAULT NOW()
);

SELECT create_hypertable('ticks', 'timestamp',
    chunk_time_interval => INTERVAL '1 hour');

CREATE INDEX idx_ticks_symbol_time
    ON ticks (symbol, timestamp DESC)
    INCLUDE (price, size, side);

ALTER TABLE ticks SET (
    timescaledb.compress,
    timescaledb.compress_orderby   = 'timestamp DESC',
    timescaledb.compress_segmentby = 'symbol'
);
SELECT add_compression_policy('ticks', INTERVAL '2 days');

-- Ticks are voluminous — retain 90 days only
SELECT add_retention_policy('ticks', INTERVAL '90 days');


-- Periodic snapshots of top-of-book state. Partition by 1 hour.
CREATE TABLE order_book_snapshots (
    symbol          VARCHAR(10)   NOT NULL REFERENCES instruments(symbol),
    best_bid        NUMERIC(14,4)          CHECK (best_bid  > 0),
    best_ask        NUMERIC(14,4)          CHECK (best_ask  > 0),
    mid_price       NUMERIC(14,4) GENERATED ALWAYS AS (
                        CASE WHEN best_bid IS NOT NULL AND best_ask IS NOT NULL
                             THEN (best_bid + best_ask) / 2.0
                        END
                    ) STORED,                               -- computed, not stored twice
    spread          NUMERIC(14,4) GENERATED ALWAYS AS (
                        CASE WHEN best_bid IS NOT NULL AND best_ask IS NOT NULL
                             THEN best_ask - best_bid
                        END
                    ) STORED,
    imbalance       NUMERIC(10,4) CHECK (imbalance BETWEEN -1 AND 1),
    total_bid_vol   BIGINT                 CHECK (total_bid_vol >= 0),
    total_ask_vol   BIGINT                 CHECK (total_ask_vol >= 0),
    snapshot_time   TIMESTAMPTZ   NOT NULL DEFAULT NOW(),

    CONSTRAINT chk_ob_bid_ask CHECK (best_bid IS NULL OR best_ask IS NULL OR best_ask >= best_bid)
);

SELECT create_hypertable('order_book_snapshots', 'snapshot_time',
    chunk_time_interval => INTERVAL '1 hour');

CREATE INDEX idx_snapshots_symbol_time
    ON order_book_snapshots (symbol, snapshot_time DESC);

ALTER TABLE order_book_snapshots SET (
    timescaledb.compress,
    timescaledb.compress_orderby   = 'snapshot_time DESC',
    timescaledb.compress_segmentby = 'symbol'
);
SELECT add_compression_policy('order_book_snapshots', INTERVAL '3 days');
SELECT add_retention_policy('order_book_snapshots', INTERVAL '90 days');


-- Computed signals attached to a symbol + time
CREATE TABLE technical_indicators (
    symbol          VARCHAR(10)   NOT NULL REFERENCES instruments(symbol),
    indicator_name  VARCHAR(20)   NOT NULL,
    value           NUMERIC(14,4) NOT NULL,
    parameters      JSONB,
    timestamp       TIMESTAMPTZ   NOT NULL DEFAULT NOW()
);

SELECT create_hypertable('technical_indicators', 'timestamp',
    chunk_time_interval => INTERVAL '1 day');

-- Composite index matches the most common query: "give me RSI(14) for AAPL last 30d"
CREATE INDEX idx_indicators_symbol_name_time
    ON technical_indicators (symbol, indicator_name, timestamp DESC);

ALTER TABLE technical_indicators SET (
    timescaledb.compress,
    timescaledb.compress_orderby   = 'timestamp DESC',
    timescaledb.compress_segmentby = 'symbol'
);
SELECT add_compression_policy('technical_indicators', INTERVAL '7 days');
SELECT add_retention_policy('technical_indicators', INTERVAL '1 year');


-- CONTINUOUS AGGREGATE: daily_ohlcv
-- Replaces the broken materialized view. TimescaleDB refreshes this
-- incrementally — only recomputes new/changed chunks, not the full dataset.
CREATE MATERIALIZED VIEW daily_ohlcv
WITH (timescaledb.continuous) AS
SELECT
    symbol,
    time_bucket('1 day', timestamp)  AS bucket,
    COUNT(*)                          AS tick_count,
    FIRST(price, timestamp)           AS open_price,-- TimescaleDB FIRST/LAST
    MAX(price)                        AS high_price,
    MIN(price)                        AS low_price,
    LAST(price, timestamp)            AS close_price,
    SUM(size)                         AS total_volume,
    AVG(price)                        AS vwap
FROM ticks
GROUP BY symbol, bucket
WITH NO DATA;

-- Refresh the last 7 days every hour. Does NOT lock the view during refresh.
SELECT add_continuous_aggregate_policy('daily_ohlcv',
    start_offset => INTERVAL '7 days',
    end_offset   => INTERVAL '1 hour',
    schedule_interval => INTERVAL '1 hour');


-- CONTINUOUS AGGREGATE: hourly_book_summary
--detecting spread/imbalance drift over time.
CREATE MATERIALIZED VIEW hourly_book_summary
WITH (timescaledb.continuous) AS
SELECT
    symbol,
    time_bucket('1 hour', snapshot_time)    AS bucket,
    AVG(spread)                             AS avg_spread,
    MIN(spread)                             AS min_spread,
    MAX(spread)                             AS max_spread,
    AVG(imbalance)                          AS avg_imbalance,
    AVG(mid_price)                          AS avg_mid
FROM order_book_snapshots
GROUP BY symbol, bucket
WITH NO DATA;

SELECT add_continuous_aggregate_policy('hourly_book_summary',
    start_offset      => INTERVAL '2 days',
    end_offset        => INTERVAL '1 hour',
    schedule_interval => INTERVAL '1 hour');


-- ROLES AND PERMISSIONS
-- Principle of least privilege. Read-only role for dashboards / analysts.
-- Write role for the C++ ingest service only.
-- Ingest service: can INSERT/SELECT, cannot DROP or ALTER
CREATE ROLE fincore_writer;
GRANT CONNECT ON DATABASE fincore TO fincore_writer;
GRANT USAGE   ON SCHEMA public TO fincore_writer;
GRANT SELECT, INSERT
    ON quotes, ticks, order_book_snapshots, technical_indicators
    TO fincore_writer;
GRANT SELECT, INSERT
    ON instruments, data_sources
    TO fincore_writer;

-- Read-only role for dashboards (pgAdmin viewers, analysts)
CREATE ROLE fincore_reader;
GRANT CONNECT ON DATABASE fincore TO fincore_reader;
GRANT USAGE   ON SCHEMA public TO fincore_reader;
GRANT SELECT
    ON ALL TABLES IN SCHEMA public
    TO fincore_reader;
-- Also grant access to continuous aggregates
GRANT SELECT ON daily_ohlcv, hourly_book_summary TO fincore_reader;

-- Application user — maps to fincore_writer
CREATE USER fincore_app WITH PASSWORD 'change_me_in_production' IN ROLE fincore_writer;

-- Analyst user — maps to fincore_reader
CREATE USER fincore_analyst WITH PASSWORD 'change_me_in_production' IN ROLE fincore_reader;

-- Ensure future tables get the same grants automatically
ALTER DEFAULT PRIVILEGES IN SCHEMA public
    GRANT SELECT, INSERT ON TABLES TO fincore_writer;
ALTER DEFAULT PRIVILEGES IN SCHEMA public
    GRANT SELECT ON TABLES TO fincore_reader;

--reference data
INSERT INTO instruments (symbol, name, asset_class, exchange) VALUES
    ('AAPL',  'Apple Inc.',            'EQUITY', 'NASDAQ'),
    ('MSFT',  'Microsoft Corporation', 'EQUITY', 'NASDAQ'),
    ('GOOGL', 'Alphabet Inc.',         'EQUITY', 'NASDAQ'),
    ('BTC',   'Bitcoin',               'CRYPTO', 'BINANCE'),
    ('ETH',   'Ethereum',              'CRYPTO', 'BINANCE')
ON CONFLICT (symbol) DO NOTHING;
