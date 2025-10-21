#include "gtest/gtest.h"
#include "ArrivalData.h"

// Tests that the bit packing works for all valid arrival delay and facing target combinations
TEST(ResendArrivalDataTests, TestPackingOfAllValidValues)
{
    auto testCase = [](unsigned int arrivalDelay, bool facingTarget)
    {
        auto test = MiningOptimizationTraining::ArrivalData::create(arrivalDelay, facingTarget);
        EXPECT_EQ(arrivalDelay, test.arrivalDelay());
        EXPECT_EQ(facingTarget, test.facingTarget());
    };
    for (int i = 0; i < 128; i++)
    {
        testCase(i, false);
        testCase(i, true);
    }
}

// Tests that extreme values are clamped
TEST(ResendArrivalDataTests, TestClampingOfExtremeValues)
{
    auto testCase = [](unsigned int arrivalDelay, unsigned int clampedArrivalDelay, bool facingTarget)
    {
        auto test = MiningOptimizationTraining::ArrivalData::create(arrivalDelay, facingTarget);
        EXPECT_EQ(clampedArrivalDelay, test.arrivalDelay());
        EXPECT_EQ(facingTarget, test.facingTarget());
    };
    testCase(128, 127, false);
    testCase(128, 127, true);
    testCase(UINT_MAX, 127, false);
    testCase(UINT_MAX, 127, true);
}
