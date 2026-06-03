#include <gtest/gtest.h>
#include "/home/artur/Desktop/Financial-Core-Streaming-System/include/storage/postgres_client.hpp"

using namespace fincore::db;

//Fresh postgres_client from scratch
class QuoteRepoTest: public ::testing::Test {
protected:
    void SetUp() override {
        fincore::db::ConnectionConfig cfg;
        cfg.host = "localhost";
        cfg.port = 5432;
        cfg.dbname = "fincore_test";
        cfg.user = "testuser";
        cfg.password = "testpassword";
        cfg.pool_size = 1;
        cfg.query_timeout_ms = 5000;

        client = std::make_unique<PostgresClient>(cfg);

        //transaction will be roll back after each test
        tx = client->begin_transaction();
    }
    //undo any insert made after test
    void TearDown() override {
        if (tx && tx->is_active())
            tx->rollback();
        client.reset();
    }

    std::unique_ptr<PostgresClient> client;
    std::unique_ptr<Transaction> tx;
};

TEST_F(QuoteRepoTest, StoreAndRetrieveQuote) {
    Quote q;
    q.symbol = "AAPL";
    q.price = 150.25;
    q.open = 149.90;
    q.high = 151.00;
    q.low = 149.80;
    q.volume = 1'000'000;
    q.change_pct = 0.23;
    q.source = "NASDAQ";
    q.timestamp = std::chrono::system_clock::now();

    // Store
    auto store_result = client->quotes().store(q.symbol, q);
    ASSERT_TRUE(store_result.ok());

    // Retrieve
    auto latest_result = client->quotes().latest(q.symbol);
    ASSERT_TRUE(latest_result.ok());
    auto opt_quote = latest_result.value();
    ASSERT_TRUE(opt_quote.has_value());

    EXPECT_EQ(opt_quote->symbol, q.symbol);
    EXPECT_DOUBLE_EQ(opt_quote->price, q.price);
    EXPECT_EQ(opt_quote->source, q.source);
    // timestamps can be compared within a few seconds
    auto diff = q.timestamp - opt_quote->timestamp;
    EXPECT_LT(std::abs(std::chrono::duration_cast<std::chrono::seconds>(diff).count()), 5);
}
