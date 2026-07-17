// tests/order_book_test.cpp

#include <gtest/gtest.h>

#include "core/order_book.hpp"

#include <chrono>
#include <map>
#include <stdexcept>
#include <string>

namespace fincore {
namespace {

class OrderBookTest : public ::testing::Test {
protected:
    OrderBook book{"BTC-USD"};
};


//Empty Book tests

TEST_F(OrderBookTest, EmptyBookHasNoBestPrices)
{
    EXPECT_FALSE(book.best_bid().has_value());
    EXPECT_FALSE(book.best_ask().has_value());
}

TEST_F(OrderBookTest, EmptyBookHasNoMidPriceOrSpread)
{
    EXPECT_FALSE(book.mid_price().has_value());
    EXPECT_FALSE(book.spread().has_value());
}

TEST_F(OrderBookTest, EmptyBookHasZeroVolumesAndImbalance)
{
    EXPECT_EQ(book.total_bid_volume(), Volume{0});
    EXPECT_EQ(book.total_ask_volume(), Volume{0});
    EXPECT_DOUBLE_EQ(book.imbalance(), 0.0);
}


//Bid Mutations
TEST_F(OrderBookTest, SetBidAddsBidLevel)
{
    book.set_bid(100.0, 25);

    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_DOUBLE_EQ(*book.best_bid(), 100.0);
    EXPECT_EQ(book.total_bid_volume(), Volume{25});
}

TEST_F(OrderBookTest, SetBidUpdatesExistingLevel)
{
    book.set_bid(100.0, 25);
    book.set_bid(100.0, 40);

    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_DOUBLE_EQ(*book.best_bid(), 100.0);
    EXPECT_EQ(book.total_bid_volume(), Volume{40});
}

TEST_F(OrderBookTest, BestBidReturnsHighestBid)
{
    book.set_bid(99.0, 10);
    book.set_bid(101.0, 20);
    book.set_bid(100.0, 30);

    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_DOUBLE_EQ(*book.best_bid(), 101.0);
}

TEST_F(OrderBookTest, ZeroBidVolumeRemovesLevel)
{
    book.set_bid(100.0, 25);
    book.set_bid(100.0, 0);

    EXPECT_FALSE(book.best_bid().has_value());
    EXPECT_EQ(book.total_bid_volume(), Volume{0});
}

TEST_F(OrderBookTest, RemovingBestBidRevealsNextBestBid)
{
    book.set_bid(101.0, 10);
    book.set_bid(100.0, 20);

    book.set_bid(101.0, 0);

    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_DOUBLE_EQ(*book.best_bid(), 100.0);
}


// Ask Mutations
TEST_F(OrderBookTest, SetAskAddsAskLevel)
{
    book.set_ask(101.0, 25);

    ASSERT_TRUE(book.best_ask().has_value());
    EXPECT_DOUBLE_EQ(*book.best_ask(), 101.0);
    EXPECT_EQ(book.total_ask_volume(), Volume{25});
}

TEST_F(OrderBookTest, SetAskUpdatesExistingLevel)
{
    book.set_ask(101.0, 25);
    book.set_ask(101.0, 50);

    ASSERT_TRUE(book.best_ask().has_value());
    EXPECT_DOUBLE_EQ(*book.best_ask(), 101.0);
    EXPECT_EQ(book.total_ask_volume(), Volume{50});
}

TEST_F(OrderBookTest, BestAskReturnsLowestAsk)
{
    book.set_ask(103.0, 10);
    book.set_ask(101.0, 20);
    book.set_ask(102.0, 30);

    ASSERT_TRUE(book.best_ask().has_value());
    EXPECT_DOUBLE_EQ(*book.best_ask(), 101.0);
}

TEST_F(OrderBookTest, ZeroAskVolumeRemovesLevel)
{
    book.set_ask(101.0, 25);
    book.set_ask(101.0, 0);

    EXPECT_FALSE(book.best_ask().has_value());
    EXPECT_EQ(book.total_ask_volume(), Volume{0});
}

TEST_F(OrderBookTest, RemovingBestAskRevealsNextBestAsk)
{
    book.set_ask(101.0, 10);
    book.set_ask(102.0, 20);

    book.set_ask(101.0, 0);

    ASSERT_TRUE(book.best_ask().has_value());
    EXPECT_DOUBLE_EQ(*book.best_ask(), 102.0);
}

//
// Input Validation
TEST_F(OrderBookTest, SetBidRejectsZeroPrice)
{
    EXPECT_THROW(book.set_bid(0.0, 10), std::invalid_argument);
}

TEST_F(OrderBookTest, SetBidRejectsNegativePrice)
{
    EXPECT_THROW(book.set_bid(-1.0, 10), std::invalid_argument);
}

TEST_F(OrderBookTest, SetAskRejectsZeroPrice)
{
    EXPECT_THROW(book.set_ask(0.0, 10), std::invalid_argument);
}

TEST_F(OrderBookTest, SetAskRejectsNegativePrice)
{
    EXPECT_THROW(book.set_ask(-1.0, 10), std::invalid_argument);
}

TEST_F(OrderBookTest, InvalidMutationDoesNotChangeExistingBook)
{
    book.set_bid(100.0, 20);
    book.set_ask(101.0, 30);

    EXPECT_THROW(book.set_bid(0.0, 50), std::invalid_argument);
    EXPECT_THROW(book.set_ask(-10.0, 50), std::invalid_argument);

    ASSERT_TRUE(book.best_bid().has_value());
    ASSERT_TRUE(book.best_ask().has_value());

    EXPECT_DOUBLE_EQ(*book.best_bid(), 100.0);
    EXPECT_DOUBLE_EQ(*book.best_ask(), 101.0);
    EXPECT_EQ(book.total_bid_volume(), Volume{20});
    EXPECT_EQ(book.total_ask_volume(), Volume{30});
}

//
// Replace operations
TEST_F(OrderBookTest, ReplaceBidsRemovesOldLevels)
{
    book.set_bid(90.0, 100);

    const std::map<Price, Volume> replacement{
        {100.0, 10},
        {101.0, 20},
        {102.0, 30},
    };

    book.replace_bids(replacement);

    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_DOUBLE_EQ(*book.best_bid(), 102.0);
    EXPECT_EQ(book.total_bid_volume(), Volume{60});
}

TEST_F(OrderBookTest, ReplaceAsksRemovesOldLevels)
{
    book.set_ask(110.0, 100);

    const std::map<Price, Volume> replacement{
        {101.0, 10},
        {102.0, 20},
        {103.0, 30},
    };

    book.replace_asks(replacement);

    ASSERT_TRUE(book.best_ask().has_value());
    EXPECT_DOUBLE_EQ(*book.best_ask(), 101.0);
    EXPECT_EQ(book.total_ask_volume(), Volume{60});
}

TEST_F(OrderBookTest, ReplaceBidsIgnoresZeroVolumeLevels)
{
    const std::map<Price, Volume> replacement{
        {100.0, 10},
        {101.0, 0},
        {102.0, 20},
    };

    book.replace_bids(replacement);

    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_DOUBLE_EQ(*book.best_bid(), 102.0);
    EXPECT_EQ(book.total_bid_volume(), Volume{30});
}

TEST_F(OrderBookTest, ReplaceAsksIgnoresZeroVolumeLevels)
{
    const std::map<Price, Volume> replacement{
        {100.0, 0},
        {101.0, 10},
        {102.0, 20},
    };

    book.replace_asks(replacement);

    ASSERT_TRUE(book.best_ask().has_value());
    EXPECT_DOUBLE_EQ(*book.best_ask(), 101.0);
    EXPECT_EQ(book.total_ask_volume(), Volume{30});
}

TEST_F(OrderBookTest, ReplacingWithEmptyMapsClearsLevels)
{
    book.set_bid(100.0, 10);
    book.set_ask(101.0, 20);

    book.replace_bids({});
    book.replace_asks({});

    EXPECT_FALSE(book.best_bid().has_value());
    EXPECT_FALSE(book.best_ask().has_value());
}

//
// Mid-price and spread
TEST_F(OrderBookTest, CalculatesMidPrice)
{
    book.set_bid(100.0, 10);
    book.set_ask(102.0, 20);

    ASSERT_TRUE(book.mid_price().has_value());
    EXPECT_DOUBLE_EQ(*book.mid_price(), 101.0);
}

TEST_F(OrderBookTest, CalculatesSpread)
{
    book.set_bid(100.0, 10);
    book.set_ask(102.0, 20);

    ASSERT_TRUE(book.spread().has_value());
    EXPECT_DOUBLE_EQ(*book.spread(), 2.0);
}

TEST_F(OrderBookTest, MidPriceAndSpreadRequireBothSides)
{
    book.set_bid(100.0, 10);

    EXPECT_FALSE(book.mid_price().has_value());
    EXPECT_FALSE(book.spread().has_value());

    book.clear();
    book.set_ask(101.0, 10);

    EXPECT_FALSE(book.mid_price().has_value());
    EXPECT_FALSE(book.spread().has_value());
}

TEST_F(OrderBookTest, CrossedBookProducesNegativeSpread)
{
    book.set_bid(102.0, 10);
    book.set_ask(101.0, 20);

    ASSERT_TRUE(book.spread().has_value());
    EXPECT_DOUBLE_EQ(*book.spread(), -1.0);

    ASSERT_TRUE(book.mid_price().has_value());
    EXPECT_DOUBLE_EQ(*book.mid_price(), 101.5);
}

//
// Aggregate volumes
TEST_F(OrderBookTest, CalculatesTotalBidVolume)
{
    book.set_bid(100.0, 10);
    book.set_bid(99.0, 20);
    book.set_bid(98.0, 30);

    EXPECT_EQ(book.total_bid_volume(), Volume{60});
}

TEST_F(OrderBookTest, CalculatesTotalAskVolume)
{
    book.set_ask(101.0, 15);
    book.set_ask(102.0, 25);
    book.set_ask(103.0, 35);

    EXPECT_EQ(book.total_ask_volume(), Volume{75});
}

// Imbalance Check
TEST_F(OrderBookTest, CalculatesBalancedImbalance)
{
    book.set_bid(100.0, 100);
    book.set_ask(101.0, 100);

    EXPECT_DOUBLE_EQ(book.imbalance(), 0.0);
}

TEST_F(OrderBookTest, CalculatesPositiveImbalance)
{
    book.set_bid(100.0, 300);
    book.set_ask(101.0, 100);

    //(300 - 100) / (300 + 100) = 0.5
    EXPECT_NEAR(book.imbalance(), 0.5, 1e-12);
}

TEST_F(OrderBookTest, CalculatesNegativeImbalance)
{
    book.set_bid(100.0, 100);
    book.set_ask(101.0, 300);

    // (100 - 300) / (100 + 300) = -0.5
    EXPECT_NEAR(book.imbalance(), -0.5, 1e-12);
}

TEST_F(OrderBookTest, BidOnlyBookHasMaximumPositiveImbalance)
{
    book.set_bid(100.0, 100);

    EXPECT_DOUBLE_EQ(book.imbalance(), 1.0);
}

TEST_F(OrderBookTest, AskOnlyBookHasMaximumNegativeImbalance)
{
    book.set_ask(101.0, 100);

    EXPECT_DOUBLE_EQ(book.imbalance(), -1.0);
}


// Clear(Removal)
TEST_F(OrderBookTest, ClearRemovesAllLevels)
{
    book.set_bid(100.0, 10);
    book.set_bid(99.0, 20);
    book.set_ask(101.0, 30);
    book.set_ask(102.0, 40);

    book.clear();

    EXPECT_FALSE(book.best_bid().has_value());
    EXPECT_FALSE(book.best_ask().has_value());
    EXPECT_FALSE(book.mid_price().has_value());
    EXPECT_FALSE(book.spread().has_value());

    EXPECT_EQ(book.total_bid_volume(), Volume{0});
    EXPECT_EQ(book.total_ask_volume(), Volume{0});
    EXPECT_DOUBLE_EQ(book.imbalance(), 0.0);
}

// j
// Snapshot Check
TEST_F(OrderBookTest, SnapshotContainsCurrentBookValues)
{
    book.set_bid(100.0, 100);
    book.set_bid(99.0, 50);

    book.set_ask(102.0, 70);
    book.set_ask(103.0, 30);

    const auto snapshot = book.snapshot();

    EXPECT_EQ(snapshot.symbol, "BTC-USD");
    EXPECT_DOUBLE_EQ(snapshot.best_bid, 100.0);
    EXPECT_DOUBLE_EQ(snapshot.best_ask, 102.0);
    EXPECT_DOUBLE_EQ(snapshot.mid_price, 101.0);
    EXPECT_DOUBLE_EQ(snapshot.spread, 2.0);

    EXPECT_EQ(snapshot.total_bid_vol, Volume{150});
    EXPECT_EQ(snapshot.total_ask_vol, Volume{100});

    EXPECT_NEAR(snapshot.imbalance, 0.2, 1e-12);
    EXPECT_NE(snapshot.snapshot_time, TimePoint{});
}

TEST_F(OrderBookTest, EmptySnapshotUsesZeroForUnavailableValues)
{
    const auto snapshot = book.snapshot();

    EXPECT_EQ(snapshot.symbol, "BTC-USD");
    EXPECT_DOUBLE_EQ(snapshot.best_bid, 0.0);
    EXPECT_DOUBLE_EQ(snapshot.best_ask, 0.0);
    EXPECT_DOUBLE_EQ(snapshot.mid_price, 0.0);
    EXPECT_DOUBLE_EQ(snapshot.spread, 0.0);

    EXPECT_EQ(snapshot.total_bid_vol, Volume{0});
    EXPECT_EQ(snapshot.total_ask_vol, Volume{0});
    EXPECT_DOUBLE_EQ(snapshot.imbalance, 0.0);
}

TEST_F(OrderBookTest, SnapshotUsesProvidedTimestamp)
{
    const TimePoint timestamp{
        std::chrono::duration_cast<TimePoint::duration>(
            std::chrono::microseconds{1'234'567})
    };

    const auto snapshot = book.snapshot(timestamp);

    EXPECT_EQ(snapshot.snapshot_time, timestamp);
}

} // namespace
} // namespace fincore
