#include "gtest/gtest.h"
#include "GatherArrivalData.h"

// Tests that the bit packing works for all valid arrival delay, facing target and collision combinations
TEST(GatherArrivalDataTests, TestPackingOfAllValidValues)
{
    auto testCase = [](unsigned int arrivalDelay, bool facingTarget, bool collision)
    {
        auto test = MiningOptimizationTraining::GatherArrivalData::create(arrivalDelay, facingTarget, collision, {});
        EXPECT_EQ(arrivalDelay, test.arrivalDelay());
        EXPECT_EQ(facingTarget, test.facingTarget());
        EXPECT_EQ(collision, test.collision());
    };
    for (unsigned int i = 0; i < 64; i++)
    {
        testCase(i, false, false);
        testCase(i, true, false);
        testCase(i, false, true);
        testCase(i, true, true);
    }
}

// Tests that extreme values are clamped
TEST(GatherArrivalDataTests, TestClampingOfExtremeValues)
{
    auto testCase =
            [](unsigned int arrivalDelay, unsigned int clampedArrivalDelay, bool facingTarget, bool collision)
            {
                auto test = MiningOptimizationTraining::GatherArrivalData::create(arrivalDelay, facingTarget, collision, {});
                EXPECT_EQ(clampedArrivalDelay, test.arrivalDelay());
                EXPECT_EQ(facingTarget, test.facingTarget());
                EXPECT_EQ(collision, test.collision());
            };
    testCase(64, 63, false, false);
    testCase(64, 63, true, false);
    testCase(64, 63, false, true);
    testCase(64, 63, true, true);
    testCase(UINT_MAX, 63, false, false);
    testCase(UINT_MAX, 63, true, false);
    testCase(UINT_MAX, 63, false, true);
    testCase(UINT_MAX, 63, true, true);
}

// Tests that setting the arrival delay works
TEST(GatherArrivalDataTests, TestSetArrivalDelay)
{
    auto testCase = [](
            unsigned int oldArrivalDelay, unsigned int newArrivalDelay, bool facingTarget, bool collision)
    {
        auto test = MiningOptimizationTraining::GatherArrivalData::create(oldArrivalDelay, facingTarget, collision, {});
        EXPECT_EQ(oldArrivalDelay, test.arrivalDelay());
        EXPECT_EQ(facingTarget, test.facingTarget());
        EXPECT_EQ(collision, test.collision());
        test.setArrivalDelay(newArrivalDelay);
        EXPECT_EQ(newArrivalDelay, test.arrivalDelay());
        EXPECT_EQ(facingTarget, test.facingTarget());
        EXPECT_EQ(collision, test.collision());
    };
    for (unsigned int i = 0; i < 64; i++)
    {
        for (unsigned int j = 0; j < 64; j++)
        {
            testCase(i, j, false, false);
            testCase(i, j, true, false);
            testCase(i, j, false, true);
            testCase(i, j, true, true);
        }
    }
}
