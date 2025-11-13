#include "gtest/gtest.h"
#include "GatherArrivalData.h"

// Tests that the bit packing works for valid arrival delay, facing target and collision combinations
TEST(GatherArrivalDataTests, TestPackingOfValidValues)
{
    auto testCase = [](unsigned int arrivalDelay, bool facingTarget, bool collision)
    {
        auto test = MiningOptimizationTraining::GatherArrivalData::create(arrivalDelay, facingTarget, collision, {});
        EXPECT_EQ(arrivalDelay, test.arrivalDelay());
        EXPECT_EQ(facingTarget, test.facingTarget());
        EXPECT_EQ(collision, test.collision());
    };
    for (unsigned int i = 0; i < 150; i++)
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
    testCase(UINT14_MAX + 1, UINT14_MAX, false, false);
    testCase(UINT14_MAX + 1, UINT14_MAX, true, false);
    testCase(UINT14_MAX + 1, UINT14_MAX, false, true);
    testCase(UINT14_MAX + 1, UINT14_MAX, true, true);
    testCase(UINT_MAX, UINT14_MAX, false, false);
    testCase(UINT_MAX, UINT14_MAX, true, false);
    testCase(UINT_MAX, UINT14_MAX, false, true);
    testCase(UINT_MAX, UINT14_MAX, true, true);
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
    for (unsigned int i = 0; i < 150; i++)
    {
        for (unsigned int j = 0; j < 150; j++)
        {
            testCase(i, j, false, false);
            testCase(i, j, true, false);
            testCase(i, j, false, true);
            testCase(i, j, true, true);
        }
    }
}
