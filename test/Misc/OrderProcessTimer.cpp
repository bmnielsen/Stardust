#include "gtest/gtest.h"

#include "OrderProcessTimer.h"

TEST(OrderProcessTimer, TestCases)
{
    EXPECT_EQ(149, OrderProcessTimer::framesToPreviousReset(157));
    EXPECT_EQ(0, OrderProcessTimer::framesToPreviousReset(158));
    EXPECT_EQ(1, OrderProcessTimer::framesToPreviousReset(159));

    EXPECT_EQ(1, OrderProcessTimer::framesToNextReset(157));
    EXPECT_EQ(0, OrderProcessTimer::framesToNextReset(158));
    EXPECT_EQ(149, OrderProcessTimer::framesToNextReset(159));

    EXPECT_EQ(8, OrderProcessTimer::previousResetFrame(157));
    EXPECT_EQ(158, OrderProcessTimer::previousResetFrame(158));
    EXPECT_EQ(158, OrderProcessTimer::previousResetFrame(159));

    EXPECT_EQ(158, OrderProcessTimer::nextResetFrame(157));
    EXPECT_EQ(158, OrderProcessTimer::nextResetFrame(158));
    EXPECT_EQ(308, OrderProcessTimer::nextResetFrame(159));
}
