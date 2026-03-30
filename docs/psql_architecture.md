# Database Design Documentation - Fincore

## Overview
This document describes the PostgreSQL + TimescaleDB schema design for the Financial Core Streaming Project. The database stores real-time market data, order book snapshots, and technical indicators with a focus on scalability, performance, and maintainability.

## Architecture Principles
- **Time-series optimized**: Leverages TimescaleDB hypertables for automatic partitioning
- **No surrogate keys**: Avoids unnecessary overhead in time-series tables
- **Compression-first**: Aggressive compression policies for older data
- **Minimal duplication**: Centralized reference tables for instruments and data sources
- **Role-based access**: Separate write and read roles for security

## Core Tables

### Reference Tables

#### `instruments`
Master list of tradeable instruments (stocks, crypto, forex, etc.)

| Column | Type | Description |
|--------|------|-------------|
| symbol | VARCHAR(10) | Primary key, unique instrument identifier |
| name | VARCHAR(100) | Full instrument name |
| asset_class | VARCHAR(20) | EQUITY, FOREX, CRYPTO, FUTURES, ETF |
| exchange | VARCHAR(20) | Trading exchange |
| tick_size_decimals | SMALLINT | Decimal places for price |
| is_active | BOOLEAN | Soft delete flag |
| created_at | TIMESTAMPTZ | Creation timestamp |
| updated_at | TIMESTAMPTZ | Last update timestamp |

#### `data_sources`
Tracks the origin of market data

| Column | Type | Description |
|--------|------|-------------|
| code | VARCHAR(20) | Primary key, source identifier |
| display_name | VARCHAR(60) | Human-readable name |
| base_url | TEXT | API endpoint (if applicable) |
| is_active | BOOLEAN | Soft delete flag |

### Time-Series Tables (Hypertables)

#### `quotes`
OHLCV (candlestick) data for all instruments

| Column | Type | Description |
|--------|------|-------------|
| symbol | VARCHAR(10) | Instrument identifier |
| price | NUMERIC(14,4) | Current/last price |
| open | NUMERIC(14,4) | Opening price |
| high | NUMERIC(14,4) | High price |
| low | NUMERIC(14,4) | Low price |
| volume | BIGINT | Trading volume |
| change_pct | NUMERIC(10,4) | Percentage change |
| source | VARCHAR(20) | Data source reference |
| timestamp | TIMESTAMPTZ | Quote timestamp |

**Partitioning**: 1-day chunks  
**Compression**: After 7 days, segmented by symbol, ordered by timestamp DESC  
**Retention**: 2 years  

#### `ticks`
Individual trade prints (high-frequency data)

| Column | Type | Description |
|--------|------|-------------|
| symbol | VARCHAR(10) | Instrument identifier |
| price | NUMERIC(14,4) | Trade price |
| size | BIGINT | Trade size/volume |
| side | CHAR(1) | B (Bid), A (Ask), U (Unknown) |
| source | VARCHAR(20) | Data source reference |
| timestamp | TIMESTAMPTZ | Trade timestamp |

**Partitioning**: 1-hour chunks  
**Compression**: After 2 days, segmented by symbol, ordered by timestamp DESC  
**Retention**: 90 days  

#### `order_book_snapshots`
Top-of-book snapshots at regular intervals

| Column | Type | Description |
|--------|------|-------------|
| symbol | VARCHAR(10) | Instrument identifier |
| best_bid | NUMERIC(14,4) | Highest bid price |
| best_ask | NUMERIC(14,4) | Lowest ask price |
| mid_price | NUMERIC(14,4) | Generated: (best_bid + best_ask) / 2 |
| spread | NUMERIC(14,4) | Generated: best_ask - best_bid |
| imbalance | NUMERIC(10,4) | Order book imbalance (-1 to 1) |
| total_bid_vol | BIGINT | Total bid volume |
| total_ask_vol | BIGINT | Total ask volume |
| snapshot_time | TIMESTAMPTZ | Snapshot timestamp |

**Partitioning**: 1-hour chunks  
**Compression**: After 3 days, segmented by symbol, ordered by snapshot_time DESC  
**Retention**: 90 days  

#### `technical_indicators`
Computed technical indicators for analysis

| Column | Type | Description |
|--------|------|-------------|
| symbol | VARCHAR(10) | Instrument identifier |
| indicator_name | VARCHAR(20) | RSI, MACD, SMA, etc. |
| value | NUMERIC(14,4) | Indicator value |
| parameters | JSONB | Indicator parameters (e.g., {"period": 14}) |
| timestamp | TIMESTAMPTZ | Calculation timestamp |

**Partitioning**: 1-day chunks  
**Compression**: After 7 days, segmented by symbol, ordered by timestamp DESC  
**Retention**: 1 year  

## Continuous Aggregates

### `daily_ohlcv`
Pre-computed daily OHLCV from tick data

- **Source**: `ticks` table
- **Refresh**: Every hour, last 7 days in real-time window
- **Use Case**: Fast daily charting without scanning millions of ticks
- **Compression**: After 30 days

### `hourly_book_summary`
Hourly aggregates of order book state

- **Source**: `order_book_snapshots` table
- **Refresh**: Every hour
- **Metrics**: Average/min/max spread, average imbalance, average mid price
- **Use Case**: Detecting spread and imbalance trends over time

## Performance Optimizations

### Indexes
- **Covering indexes**: `idx_quotes_symbol_time` includes price/volume to avoid table access
- **Time-based indexes**: Fast time-range queries across all symbols
- **Composite indexes**: Optimized for per-symbol time-series queries

### Compression Strategy
- **Segment by symbol**: Per-symbol data stored contiguously
- **Order by timestamp DESC**: Recent data first for common queries
- **Compression ratio**: 10-20x storage reduction for historical data
- **Compression policy**: Oldest chunks compressed automatically

### Partitioning
- **Quotes**: Daily chunks (balances query performance and chunk count)
- **Ticks/Hourly snapshots**: Hourly chunks (manages high-frequency data)

## Security Model

### Roles

| Role | Privileges | Purpose |
|------|------------|---------|
| `fincore_writer` | INSERT, SELECT on all tables | Application ingest service |
| `fincore_reader` | SELECT on all tables | Dashboards, analysts, queries |

### Users
- `fincore_app`: Application user (member of `fincore_writer`)
- `fincore_analyst`: Read-only analyst user (member of `fincore_reader`)

### Default Privileges
- Future tables automatically get proper grants
- Prevents privilege escalation when adding new tables

## Maintenance Operations

### Automated Policies
| Policy | Table | Interval |
|--------|-------|----------|
| Compression | quotes | 7 days |
| Compression | ticks | 2 days |
| Compression | snapshots | 3 days |
| Compression | indicators | 7 days |
| Retention | quotes | 2 years |
| Retention | ticks | 90 days |
| Retention | snapshots | 90 days |
| Retention | indicators | 1 year |

### Manual Maintenance
- **Vacuum**: Automatic via autovacuum, tune for write-heavy workloads
- **Analyze**: Regular statistics updates for query planner
- **Monitoring**: Use `pg_stat_statements` for slow query detection
