#include "gtest/gtest.h"
#include "ReturnArrivalData.h"

// Tests that the bit packing works for valid arrival delay and exit speed
TEST(ReturnArrivalDataTests, TestPackingOfValidValues)
{
    auto testCase = [](unsigned int arrivalDelay, MiningOptimizationTraining::ReturnExitSpeed exitSpeed)
    {
        auto test = MiningOptimizationTraining::ReturnArrivalData::create(arrivalDelay, exitSpeed, {});
        EXPECT_EQ(arrivalDelay, test.arrivalDelay());
        EXPECT_EQ(exitSpeed, test.exitSpeed());
    };
    for (unsigned int i = 0; i < 150; i++)
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
        testCase(UINT14_MAX + 1, UINT14_MAX, (MiningOptimizationTraining::ReturnExitSpeed)i);
        testCase(UINT_MAX, UINT14_MAX, (MiningOptimizationTraining::ReturnExitSpeed)i);
    }
}

// Tests that setting the arrival delay works
TEST(ReturnArrivalDataTests, TestSetArrivalDelay)
{
    auto testCase = [](
            unsigned int oldArrivalDelay, unsigned int newArrivalDelay, MiningOptimizationTraining::ReturnExitSpeed exitSpeed)
    {
        auto test = MiningOptimizationTraining::ReturnArrivalData::create(oldArrivalDelay, exitSpeed, {});
        EXPECT_EQ(oldArrivalDelay, test.arrivalDelay());
        EXPECT_EQ(exitSpeed, test.exitSpeed());
        test.setArrivalDelay(newArrivalDelay);
        EXPECT_EQ(newArrivalDelay, test.arrivalDelay());
        EXPECT_EQ(exitSpeed, test.exitSpeed());
    };
    for (unsigned int i = 0; i < 150; i++)
    {
        for (unsigned int j = 0; j < 150; j++)
        {
            for (uint8_t k = 0; k < 4; k++)
            {
                testCase(i, j, (MiningOptimizationTraining::ReturnExitSpeed)k);
            }
        }
    }
}
