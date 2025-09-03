#include "BWTest.h"
#include "DoNothingModule.h"

#include "OrderProcessTimer.h"

TEST(OrderProcessTimer, TestCases)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.map = Maps::GetOne("Tau Cross");
    test.randomSeed = 42;
    test.frameLimit = 10;
    test.expectWin = false;
    test.writeReplay = false;

    test.onFrameMine = []()
    {
        if (BWAPI::Broodwar->getFrameCount() != 5) return;

        // Set the bot frame to the engine frame so the results make sense conceptually
        // In reality the bot runs with currentFrame one frame behind to make it easier to reason with what data it is seeing
        // (since the bot is invoked with the results of the previous frame's engine updates)
        currentFrame = 5;

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

        EXPECT_EQ(8, OrderProcessTimer::unitOrderProcessTimerAtDelta(100, 0, 1));
        EXPECT_EQ(0, OrderProcessTimer::unitOrderProcessTimerAtDelta(100, 8, -1));

        EXPECT_EQ(-1, OrderProcessTimer::unitOrderProcessTimerAtDelta(156, 5, 2));
        EXPECT_EQ(-1, OrderProcessTimer::unitOrderProcessTimerAtDelta(157, 5, 1));
        EXPECT_EQ(4, OrderProcessTimer::unitOrderProcessTimerAtDelta(158, 5, 1));

        EXPECT_EQ(6, OrderProcessTimer::unitOrderProcessTimerAtDelta(157, 5, -1));
        EXPECT_EQ(-1, OrderProcessTimer::unitOrderProcessTimerAtDelta(158, 5, -1));
        EXPECT_EQ(-1, OrderProcessTimer::unitOrderProcessTimerAtDelta(159, 5, -2));

        for (int i = 0; i < 9; i++)
        {
            EXPECT_EQ(i, OrderProcessTimer::unitOrderProcessTimerAtDelta(100, i, 9));
            EXPECT_EQ(i, OrderProcessTimer::unitOrderProcessTimerAtDelta(100, i, -9));
            EXPECT_EQ(i, OrderProcessTimer::unitOrderProcessTimerAtDelta(100, i, 18));
            EXPECT_EQ(i, OrderProcessTimer::unitOrderProcessTimerAtDelta(100, i, -18));
        }
    };

    test.run();
}
