#include <gtest/gtest.h>

#include <api/alpha_vantage_client.hpp>

#include <chrono>
#include <vector>
#include <string>
#include <stdexcept>
#include <cstdlib>

namespace fincore{
namespace {

constexpr const char* VALID_QUOTE_JSON = R"json(
{
    "Global Quote": {
        "01. symbol": "IBM",
        "02. open": "100.25",
        "03. high": "105.75",
        "04. low": "99.50",
        "05. price": "104.50",
        "06. volume": "123456",
        "07. latest trading day": "2026-07-14",
        "08. previous close": "101.00",
        "09. change": "3.50",
        "10. change percent": "3.4653%"
    }
}
)json";

constexpr const char* SECOND_QUOTE_JSON = R"json(
{
    "Global Quote": {
        "01. symbol": "IBM",
        "02. open": "200.00",
        "03. high": "210.00",
        "04. low": "195.00",
        "05. price": "205.00",
        "06. volume": "500",
        "07. latest trading day": "2026-07-15",
        "10. change percent": "2.5%"
    }
}
)json";


//test object construction
TEST(AlphaVantageClientTest, RejectEmptyApiKey)
{
    EXPECT_THROW(
            AlphaVantageClient("", std::chrono::seconds{60}),
            std::invalid_argument);
}

TEST(AlphaVantageClientTest, AcceptNonEmptyApiKey)
{
    EXPECT_NO_THROW(
            AlphaVantageClient("test-api-key", std::chrono::seconds{60}));
}


//Basic quote parsing through the public api
//
TEST(AlphaVantageClientTest, ParsingValidQuote)
{
    int fetch_count =0;
    AlphaVantageClient client(
        "test_api_key",
        std::chrono::seconds{60},
        [&](const Symbol& symbol) {
            ++fetch_count;
            EXPECT_EQ(symbol, "IBM");
            return std::string{VALID_QUOTE_JSON};
        });

    const auto quote = client.get_quote("IBM");

    ASSERT_TRUE(quote.has_value());

    EXPECT_EQ(quote->symbol, "IBM");
    EXPECT_DOUBLE_EQ(quote->price, 104.50);
    EXPECT_DOUBLE_EQ(quote->open, 100.25);
    EXPECT_DOUBLE_EQ(quote->high, 105.75);
    EXPECT_DOUBLE_EQ(quote->low, 99.50);
    EXPECT_EQ(quote->volume, Volume{123456});
    EXPECT_NEAR(quote->change_pct, 3.4653, 1e-12);

    EXPECT_EQ(fetch_count, 1);
    EXPECT_FALSE(client.last_was_cached());
}

TEST(AlphaVantageClientTest, UsesRequestedSymbol)
{
    AlphaVantageClient client(
        "test-key",
        std::chrono::seconds{60},
        [](const Symbol&) {
            return std::string(VALID_QUOTE_JSON);
        });

    //Im trying to reassign requested symbol to another one
    const auto quote = client.get_quote("AAPL");
    ASSERT_TRUE(quote.has_value());
    EXPECT_EQ(quote->symbol, "AAPL");
}

TEST(AlphaVantageClientTest, EmptySymbolDoesNotCallFetcher)
{
    int fetch_count = 0;
    AlphaVantageClient client(
        "test-key",
        std::chrono::seconds{60},
        [&](const Symbol&) {
            ++fetch_count;
            return std::string{VALID_QUOTE_JSON};
        });

        const auto quote = client.get_quote("");

        EXPECT_FALSE(quote.has_value());
        EXPECT_EQ(fetch_count, 0);
        EXPECT_FALSE(client.last_was_cached());
}

TEST(AlphaVantageClientTest, EmptyResponseReturnsNullopt)
{
    AlphaVantageClient client(
        "test-key",
        std::chrono::seconds{60},
        [](const Symbol&) {
            return std::string{};
        });

    const auto quote = client.get_quote("IBM");

    EXPECT_FALSE(quote.has_value());
    EXPECT_FALSE(client.last_was_cached());
}


//Some required fields

TEST(AlphaVantageClientTest, MissingPriceReturnsNullopt)
{
    constexpr const char* json = R"json(
    {
        "Global Quote": {
            "02. open": "100.25",
            "03. high": "105.75",
            "04. low": "99.50"
        }
    }
    )json";

    AlphaVantageClient client(
        "test-key",
        std::chrono::seconds{60},
        [](const Symbol&) {
            return std::string{json};
        });

    EXPECT_FALSE(client.get_quote("IBM").has_value());
}

TEST(AlphaVantageClientTest, MissingOpenReturnsNullopt)
{
    constexpr const char* json = R"json(
    {
        "Global Quote": {
            "03. high": "105.75",
            "04. low": "99.50",
            "05. price": "104.50"
        }
    }
    )json";

    AlphaVantageClient client(
        "test-key",
        std::chrono::seconds{60},
        [](const Symbol&) {
            return std::string{json};
        });

    EXPECT_FALSE(client.get_quote("IBM").has_value());
}

TEST(AlphaVantageClientTest, InvalidRequiredNumberReturnsNullopt)
{
    constexpr const char* json = R"json(
    {
        "Global Quote": {
            "02. open": "not-a-number",
            "03. high": "105.75",
            "04. low": "99.50",
            "05. price": "104.50"
        }
    }
    )json";

    AlphaVantageClient client(
        "test-key",
        std::chrono::seconds{60},
        [](const Symbol&) {
            return std::string{json};
        });

    EXPECT_FALSE(client.get_quote("IBM").has_value());
}


//Optional fields
//
TEST(AlphaVantageClientTest, InvalidVolumeBecomesZero)
{
    constexpr const char* json = R"json(
    {
        "Global Quote": {
            "02. open": "100.25",
            "03. high": "105.75",
            "04. low": "99.50",
            "05. price": "104.50",
            "06. volume": "invalid"
        }
    }
    )json";

    AlphaVantageClient client(
        "test-key",
        std::chrono::seconds{60},
        [](const Symbol&) {
            return std::string{json};
        });

    const auto quote = client.get_quote("IBM");

    ASSERT_TRUE(quote.has_value());
    EXPECT_EQ(quote->volume, Volume{0});
}

TEST(AlphaVantageClientTest, InvalidChangePercentBecomesZero)
{
    constexpr const char* json = R"json(
    {
        "Global Quote": {
            "02. open": "100.25",
            "03. high": "105.75",
            "04. low": "99.50",
            "05. price": "104.50",
            "10. change percent": "invalid%"
        }
    }
    )json";

    AlphaVantageClient client(
        "test-key",
        std::chrono::seconds{60},
        [](const Symbol&) {
            return std::string{json};
        });

    const auto quote = client.get_quote("IBM");

    ASSERT_TRUE(quote.has_value());
    EXPECT_DOUBLE_EQ(quote->change_pct, 0.0);
}

TEST(AlphaVantageClientTest, ParsesChangePercentWithoutPercentSign)
{
    constexpr const char* json = R"json(
    {
        "Global Quote": {
            "02. open": "100.25",
            "03. high": "105.75",
            "04. low": "99.50",
            "05. price": "104.50",
            "10. change percent": "-1.75"
        }
    }
    )json";

    AlphaVantageClient client(
        "test-key",
        std::chrono::seconds{60},
        [](const Symbol&) {
            return std::string{json};
        });

    const auto quote = client.get_quote("IBM");

    ASSERT_TRUE(quote.has_value());
    EXPECT_DOUBLE_EQ(quote->change_pct, -1.75);
}


//Cache Behaviour
//
TEST(AlphaVantageClientTest, SecondRequestUsesCache)
{
    int fetch_count = 0;
    AlphaVantageClient client (
        "test-key",
        std::chrono::seconds{3600},
        [&](const Symbol&) {
            ++fetch_count; return std::string{VALID_QUOTE_JSON};
        });
    const auto first = client.get_quote("IBM");

    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(fetch_count, 1);
    EXPECT_FALSE(client.last_was_cached());

    const auto second = client.get_quote("IBM");

    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(fetch_count, 1);
    EXPECT_TRUE(client.last_was_cached());

    EXPECT_DOUBLE_EQ(first->price, second->price);
}

TEST(AlphaVantageClientTest, DifferentSymbolHaveDifferentCacheEntries)
{
    int fetch_count = 0;

    AlphaVantageClient client(
        "test-key",
        std::chrono::seconds{3600},
        [&](const Symbol&) {
            ++fetch_count;
            return std::string{VALID_QUOTE_JSON};
        });

    ASSERT_TRUE(client.get_quote("IBM").has_value());
    EXPECT_FALSE(client.last_was_cached());

    ASSERT_TRUE(client.get_quote("AAPL").has_value());
    EXPECT_FALSE(client.last_was_cached());

    EXPECT_EQ(fetch_count, 2);

    ASSERT_TRUE(client.get_quote("IBM").has_value());
    EXPECT_TRUE(client.last_was_cached());

    ASSERT_TRUE(client.get_quote("AAPL").has_value());
    EXPECT_TRUE(client.last_was_cached());

    EXPECT_EQ(fetch_count, 2);
}

TEST(AlphaVantageClientTest, ZeroTtlAlwaysFetches)
{
    int fetch_count = 0;

    AlphaVantageClient client(
        "test-key",
        std::chrono::seconds{0},
        [&](const Symbol&) {
            ++fetch_count;
            return std::string{VALID_QUOTE_JSON};
        });

    ASSERT_TRUE(client.get_quote("IBM").has_value());
    EXPECT_FALSE(client.last_was_cached());

    ASSERT_TRUE(client.get_quote("IBM").has_value());
    EXPECT_FALSE(client.last_was_cached());

    EXPECT_EQ(fetch_count, 2);
}

TEST(AlphaVantageClientTest, FreshRequestBypassesCache)
{
    int fetch_count = 0;

    AlphaVantageClient client(
        "test-key",
        std::chrono::seconds{3600},
        [&](const Symbol&) {
            ++fetch_count;

            if (fetch_count == 1) {
                return std::string{VALID_QUOTE_JSON};
            }

            return std::string{SECOND_QUOTE_JSON};
        });

    const auto first = client.get_quote("IBM");

    ASSERT_TRUE(first.has_value());
    EXPECT_DOUBLE_EQ(first->price, 104.50);
    EXPECT_EQ(fetch_count, 1);

    const auto cached = client.get_quote("IBM");

    ASSERT_TRUE(cached.has_value());
    EXPECT_DOUBLE_EQ(cached->price, 104.50);
    EXPECT_TRUE(client.last_was_cached());
    EXPECT_EQ(fetch_count, 1);

    const auto fresh = client.get_quote_fresh("IBM");

    ASSERT_TRUE(fresh.has_value());
    EXPECT_DOUBLE_EQ(fresh->price, 205.00);
    EXPECT_FALSE(client.last_was_cached());
    EXPECT_EQ(fetch_count, 2);

    // The fresh response should replace the old cached response.
    const auto updated_cache = client.get_quote("IBM");

    ASSERT_TRUE(updated_cache.has_value());
    EXPECT_DOUBLE_EQ(updated_cache->price, 205.00);
    EXPECT_TRUE(client.last_was_cached());
    EXPECT_EQ(fetch_count, 2);
}

TEST(AlphaVantageClientTest, FailedResponseIsNotCached)
{
    int fetch_count = 0;

    AlphaVantageClient client(
        "test-key",
        std::chrono::seconds{3600},
        [&](const Symbol&) {
            ++fetch_count;

            if (fetch_count == 1) {
                return std::string{"invalid JSON"};
            }

            return std::string{VALID_QUOTE_JSON};
        });

    EXPECT_FALSE(client.get_quote("IBM").has_value());
    EXPECT_FALSE(client.last_was_cached());
    EXPECT_EQ(fetch_count, 1);

    const auto second = client.get_quote("IBM");

    ASSERT_TRUE(second.has_value());
    EXPECT_FALSE(client.last_was_cached());
    EXPECT_EQ(fetch_count, 2);

    const auto third = client.get_quote("IBM");

    ASSERT_TRUE(third.has_value());
    EXPECT_TRUE(client.last_was_cached());
    EXPECT_EQ(fetch_count, 2);
}

TEST(AlphaVantageClientTest, EmptySymbolResetsLastCachedStatus)
{
    AlphaVantageClient client(
        "test-key",
        std::chrono::seconds{3600},
        [](const Symbol&) {
            return std::string{VALID_QUOTE_JSON};
        });

    ASSERT_TRUE(client.get_quote("IBM").has_value());
    ASSERT_TRUE(client.get_quote("IBM").has_value());
    ASSERT_TRUE(client.last_was_cached());

    EXPECT_FALSE(client.get_quote("").has_value());
    EXPECT_FALSE(client.last_was_cached());
}




}
}


