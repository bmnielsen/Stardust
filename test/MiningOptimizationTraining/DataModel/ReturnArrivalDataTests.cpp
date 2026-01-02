#include "gtest/gtest.h"
#include "ReturnArrivalData.h"

// Tests that the bit packing works for valid arrival delay and exit speed
TEST(ReturnArrivalDataTests, TestPackingOfValidValues)
{
    auto testCase = [](
            unsigned int arrivalDelay,
            MiningOptimizationTraining::ReturnExitSpeed exitSpeed,
            bool collision)
    {
        auto test = MiningOptimizationTraining::ReturnArrivalData::create(arrivalDelay, exitSpeed, collision, {}, {});
        EXPECT_EQ(arrivalDelay, test.arrivalDelay());
        EXPECT_EQ(exitSpeed, test.exitSpeed());
        EXPECT_EQ(collision, test.collision());
    };
    for (unsigned int i = 0; i < 150; i++)
    {
        for (uint8_t j = 0; j < 4; j++)
        {
            testCase(i, (MiningOptimizationTraining::ReturnExitSpeed)j, true);
            testCase(i, (MiningOptimizationTraining::ReturnExitSpeed)j, false);
        }
    }
}

// Tests that extreme values are clamped
TEST(ReturnArrivalDataTests, TestClampingOfExtremeValues)
{
    auto testCase = [](
            unsigned int arrivalDelay,
            unsigned int clampedArrivalDelay,
            MiningOptimizationTraining::ReturnExitSpeed exitSpeed,
            bool collision)
    {
        auto test = MiningOptimizationTraining::ReturnArrivalData::create(arrivalDelay, exitSpeed, collision, {}, {});
        EXPECT_EQ(clampedArrivalDelay, test.arrivalDelay());
        EXPECT_EQ(exitSpeed, test.exitSpeed());
        EXPECT_EQ(collision, test.collision());
    };
    for (uint8_t i = 0; i < 4; i++)
    {
        testCase(UINT13_MAX + 1, UINT13_MAX, (MiningOptimizationTraining::ReturnExitSpeed)i, true);
        testCase(UINT13_MAX + 1, UINT13_MAX, (MiningOptimizationTraining::ReturnExitSpeed)i, false);
        testCase(UINT_MAX, UINT13_MAX, (MiningOptimizationTraining::ReturnExitSpeed)i, true);
        testCase(UINT_MAX, UINT13_MAX, (MiningOptimizationTraining::ReturnExitSpeed)i, false);
    }
}

// Tests that setting the arrival delay works
TEST(ReturnArrivalDataTests, TestSetArrivalDelay)
{
    auto testCase = [](
            unsigned int oldArrivalDelay,
            unsigned int newArrivalDelay,
            MiningOptimizationTraining::ReturnExitSpeed exitSpeed,
            bool collision)
    {
        auto test = MiningOptimizationTraining::ReturnArrivalData::create(oldArrivalDelay, exitSpeed, collision, {}, {});
        EXPECT_EQ(oldArrivalDelay, test.arrivalDelay());
        EXPECT_EQ(exitSpeed, test.exitSpeed());
        EXPECT_EQ(collision, test.collision());
        test.setArrivalDelay(newArrivalDelay);
        EXPECT_EQ(newArrivalDelay, test.arrivalDelay());
        EXPECT_EQ(exitSpeed, test.exitSpeed());
        EXPECT_EQ(collision, test.collision());
    };
    for (unsigned int i = 0; i < 150; i++)
    {
        for (unsigned int j = 0; j < 150; j++)
        {
            for (uint8_t k = 0; k < 4; k++)
            {
                testCase(i, j, (MiningOptimizationTraining::ReturnExitSpeed)k, true);
                testCase(i, j, (MiningOptimizationTraining::ReturnExitSpeed)k, false);
            }
        }
    }
}
