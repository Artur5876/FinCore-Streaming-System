// tests/order_book_test.cpp

#include <gtest/gtest.h>

#include "core/order_book.hpp"

#include <chrono>
#include <map>
#include <stdexcept>
#include <string>

namespace fincore {
namespace {

class OrderBookTest {
protected:
    OrderBook book{"BTC-USD"};
};


//Empty Book tests

TEST(OrderBookTest, EmptyBookHasNoBestPrices)
{
    EXPECT_FALSE(book.best_bid().has_value());
    EXPECT_FALSE(book.best_ask().has_value());
}

TEST(OrderBookTest, EmptyBookHasNoMidPriceOrSpread)
{
    EXPECT_FALSE(book.mid_price().has_value());
    EXPECT_FALSE(book.spread().has_value());
}

TEST(OrderBookTest, EmptyBookHasZeroVolumesAndImbalance)
{
    EXPECT_EQ(book.total_bid_volume(), Volume{0});
    EXPECT_EQ(book.total_ask_volume(), Volume{0});
    EXPECT_DOUBLE_EQ(book.imbalance(), 0.0);
}


//Bid Mutations
TEST(OrderBookTest, SetBidAddsBidLevel)
{
    book.set_bid(100.0, 25);

    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_DOUBLE_EQ(*book.best_bid(), 100.0);
    EXPECT_EQ(book.total_bid_volume(), Volume{25});
}

TEST(OrderBookTest, SetBidUpdatesExistingLevel)
{
    book.set_bid(100.0, 25);
    book.set_bid(100.0, 40);

    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_DOUBLE_EQ(*book.best_bid(), 100.0);
    EXPECT_EQ(book.total_bid_volume(), Volume{40});
}

TEST(OrderBookTest, BestBidReturnsHighestBid)
{
    book.set_bid(99.0, 10);
    book.set_bid(101.0, 20);
    book.set_bid(100.0, 30);

    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_DOUBLE_EQ(*book.best_bid(), 101.0);
}

TEST(OrderBookTest, ZeroBidVolumeRemovesLevel)
{
    book.set_bid(100.0, 25);
    book.set_bid(100.0, 0);

    EXPECT_FALSE(book.best_bid().has_value());
    EXPECT_EQ(book.total_bid_volume(), Volume{0});
}

TEST(OrderBookTest, RemovingBestBidRevealsNextBestBid)
{
    book.set_bid(101.0, 10);
    book.set_bid(100.0, 20);

    book.set_bid(101.0, 0);

    ASSERT_TRUE(book.best_bid().has_value());
    EXPECT_DOUBLE_EQ(*book.best_bid(), 100.0);
}


// Ask Mutations
TEST(OrderBookTest, SetAskAddsAskLevel)
{
    book.set_ask(101.0, 25);

    ASSERT_TRUE(book.best_ask().has_value());
    EXPECT_DOUBLE_EQ(*book.best_ask(), 101.0);
    EXPECT_EQ(book.total_ask_volume(), Volume{25});
}

TEST(OrderBookTest, SetAskUpdatesExistingLevel)
{
    book.set_ask(101.0, 25);
    book.set_ask(101.0, 50);

    ASSERT_TRUE(book.best_ask().has_value());
    EXPECT_DOUBLE_EQ(*book.best_ask(), 101.0);
    EXPECT_EQ(book.total_ask_volume(), Volume{50});
}

TEST(OrderBookTest, BestAskReturnsLowestAsk)
{
    book.set_ask(103.0, 10);
    book.set_ask(101.0, 20);
    book.set_ask(102.0, 30);

    ASSERT_TRUE(book.best_ask().has_value());
    EXPECT_DOUBLE_EQ(*book.best_ask(), 101.0);
}

TEST(OrderBookTest, ZeroAskVolumeRemovesLevel)
{
    book.set_ask(101.0, 25);
    book.set_ask(101.0, 0);

    EXPECT_FALSE(book.best_ask().has_value());
    EXPECT_EQ(book.total_ask_volume(), Volume{0});
}

TEST(OrderBookTest, RemovingBestAskRevealsNextBestAsk)
{
    book.set_ask(101.0, 10);
    book.set_ask(102.0, 20);

    book.set_ask(101.0, 0);

    ASSERT_TRUE(book.best_ask().has_value());
    EXPECT_DOUBLE_EQ(*book.best_ask(), 102.0);
}

//
// Input Validation
TEST(OrderBookTest, SetBidRejectsZeroPrice)
{
    EXPECT_THROW(book.set_bid(0.0, 10), std::invalid_argument);
}

TEST(OrderBookTest, SetBidRejectsNegativePrice)
{
    EXPECT_THROW(book.set_bid(-1.0, 10), std::invalid_argument);
}

TEST(OrderBookTest, SetAskRejectsZeroPrice)
{
    EXPECT_THROW(book.set_ask(0.0, 10), std::invalid_argument);
}

TEST(OrderBookTest, SetAskRejectsNegativePrice)
{
    EXPECT_THROW(book.set_ask(-1.0, 10), std::invalid_argument);
}

TEST(OrderBookTest, InvalidMutationDoesNotChangeExistingBook)
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
TEST(OrderBookTest, ReplaceBidsRemovesOldLevels)
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

TEST(OrderBookTest, ReplaceAsksRemovesOldLevels)
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

TEST(OrderBookTest, ReplaceBidsIgnoresZeroVolumeLevels)
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

TEST(OrderBookTest, ReplaceAsksIgnoresZeroVolumeLevels)
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

TEST(OrderBookTest, ReplacingWithEmptyMapsClearsLevels)
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
TEST(OrderBookTest, CalculatesMidPrice)
{
    book.set_bid(100.0, 10);
    book.set_ask(102.0, 20);

    ASSERT_TRUE(book.mid_price().has_value());
    EXPECT_DOUBLE_EQ(*book.mid_price(), 101.0);
}

TEST(OrderBookTest, CalculatesSpread)
{
    book.set_bid(100.0, 10);
    book.set_ask(102.0, 20);

    ASSERT_TRUE(book.spread().has_value());
    EXPECT_DOUBLE_EQ(*book.spread(), 2.0);
}

TEST(OrderBookTest, MidPriceAndSpreadRequireBothSides)
{
    book.set_bid(100.0, 10);

    EXPECT_FALSE(book.mid_price().has_value());
    EXPECT_FALSE(book.spread().has_value());

    book.clear();
    book.set_ask(101.0, 10);

    EXPECT_FALSE(book.mid_price().has_value());
    EXPECT_FALSE(book.spread().has_value());
}

TEST(OrderBookTest, CrossedBookProducesNegativeSpread)
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
TEST(OrderBookTest, CalculatesTotalBidVolume)
{
    book.set_bid(100.0, 10);
    book.set_bid(99.0, 20);
    book.set_bid(98.0, 30);

    EXPECT_EQ(book.total_bid_volume(), Volume{60});
}

TEST(OrderBookTest, CalculatesTotalAskVolume)
{
    book.set_ask(101.0, 15);
    book.set_ask(102.0, 25);
    book.set_ask(103.0, 35);

    EXPECT_EQ(book.total_ask_volume(), Volume{75});
}

// Imbalance Check
TEST(OrderBookTest, CalculatesBalancedImbalance)
{
    book.set_bid(100.0, 100);
    book.set_ask(101.0, 100);

    EXPECT_DOUBLE_EQ(book.imbalance(), 0.0);
}

TEST(OrderBookTest, CalculatesPositiveImbalance)
{
    book.set_bid(100.0, 300);
    book.set_ask(101.0, 100);

    //(300 - 100) / (300 + 100) = 0.5
    EXPECT_NEAR(book.imbalance(), 0.5, 1e-12);
}

TEST(OrderBookTest, CalculatesNegativeImbalance)
{
    book.set_bid(100.0, 100);
    book.set_ask(101.0, 300);

    // (100 - 300) / (100 + 300) = -0.5
    EXPECT_NEAR(book.imbalance(), -0.5, 1e-12);
}

TEST(OrderBookTest, BidOnlyBookHasMaximumPositiveImbalance)
{
    book.set_bid(100.0, 100);

    EXPECT_DOUBLE_EQ(book.imbalance(), 1.0);
}

TEST(OrderBookTest, AskOnlyBookHasMaximumNegativeImbalance)
{
    book.set_ask(101.0, 100);

    EXPECT_DOUBLE_EQ(book.imbalance(), -1.0);
}


// Clear(Removal)
TEST(OrderBookTest, ClearRemovesAllLevels)
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
TEST(OrderBookTest, SnapshotContainsCurrentBookValues)
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

TEST(OrderBookTest, EmptySnapshotUsesZeroForUnavailableValues)
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

TEST(OrderBookTest, SnapshotUsesProvidedTimestamp)
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
