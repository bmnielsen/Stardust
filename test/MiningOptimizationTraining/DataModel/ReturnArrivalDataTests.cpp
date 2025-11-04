#include "gtest/gtest.h"
#include "ReturnArrivalData.h"

// Tests that the bit packing works for all valid arrival delay and exit speed
TEST(ReturnArrivalDataTests, TestPackingOfAllValidValues)
{
    auto testCase = [](unsigned int arrivalDelay, MiningOptimizationTraining::ReturnExitSpeed exitSpeed)
    {
        auto test = MiningOptimizationTraining::ReturnArrivalData::create(arrivalDelay, exitSpeed, {});
        EXPECT_EQ(arrivalDelay, test.arrivalDelay());
        EXPECT_EQ(exitSpeed, test.exitSpeed());
    };
    for (unsigned int i = 0; i < 64; i++)
    {
        for (uint8_t j = 0; j < 4; j++)
        {
            testCase(i, (MiningOptimizationTraining::ReturnExitSpeed)j);
        }
    }
}

// Tests that extreme values are clamped
TEST(ReturnArrivalDataTests, TestClampingOfExtremeValues)
{
    auto testCase =
            [](unsigned int arrivalDelay, unsigned int clampedArrivalDelay, MiningOptimizationTraining::ReturnExitSpeed exitSpeed)
            {
                auto test = MiningOptimizationTraining::ReturnArrivalData::create(arrivalDelay, exitSpeed, {});
                EXPECT_EQ(clampedArrivalDelay, test.arrivalDelay());
                EXPECT_EQ(exitSpeed, test.exitSpeed());
            };
    for (uint8_t i = 0; i < 4; i++)
    {
        testCase(64, 63, (MiningOptimizationTraining::ReturnExitSpeed)i);
        testCase(UINT_MAX, 63, (MiningOptimizationTraining::ReturnExitSpeed)i);
    }
}
