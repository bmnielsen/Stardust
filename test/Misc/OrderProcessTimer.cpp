#include "BWTest.h"
#include "DoNothingModule.h"
#include "InstrumentedDoNothingModule.h"

#include "OrderProcessTimer.h"

namespace
{
    // Reset frames are 1508, 1658, 1808, 1958, 2108, 2258, 2408, 2558
    std::set<int> gatherCommandFrames = {1502, 1550, 1653, 1804, 1955};
    std::set<int> returnCommandFrames = {2102, 2150, 2253, 2404, 2555};

    class AtStartOfFrameAtDeltaModule : public InstrumentedDoNothingModule
    {
    public:
        std::vector<std::pair<BWAPI::Unit, BWAPI::Unit>> workersAndPatches;
        std::map<BWAPI::Unit, std::map<int, int>> workerToFrameToActualOrderProcessTimer;

        AtStartOfFrameAtDeltaModule() : InstrumentedDoNothingModule(false) {}

        void onFrame() override
        {
            InstrumentedDoNothingModule::onFrameStart();

            if (BWAPI::Broodwar->getFrameCount() == 1)
            {
                // Assign each worker to a patch and send it to gather
                std::set<BWAPI::Unit> usedPatches;
                for (auto worker : BWAPI::Broodwar->self()->getUnits())
                {
                    if (!worker->getType().isWorker()) continue;

                    BWAPI::Unit bestPatch = nullptr;
                    int bestDist = INT_MAX;
                    for (auto patch : BWAPI::Broodwar->neutral()->getUnits())
                    {
                        if (!patch->getType().isMineralField()) continue;
                        if (usedPatches.contains(patch)) continue;

                        int dist = worker->getDistance(patch);
                        if (dist < bestDist)
                        {
                            bestPatch = patch;
                            bestDist = dist;
                        }
                    }

                    if (bestPatch)
                    {
                        workersAndPatches.emplace_back(worker, bestPatch);
                        usedPatches.insert(bestPatch);

                        worker->gather(bestPatch);
                    }
                }
            }
            else if (BWAPI::Broodwar->getFrameCount() < 1490)
            {
                // Once each worker is carrying minerals, order it to move to the top-right corner of the map
                for (auto &[worker, _] : workersAndPatches)
                {
                    if (!worker->isCarryingMinerals()) continue;
                    if (worker->getOrder() == BWAPI::Orders::Move) continue;
                    if (worker->getLastCommandFrame() >= (BWAPI::Broodwar->getFrameCount() - BWAPI::Broodwar->getLatencyFrames())) continue;
                    worker->move(BWAPI::Position(BWAPI::Broodwar->mapWidth() * 32 - 64, 64));
                }
            }
            else if (BWAPI::Broodwar->getFrameCount() == 1490)
            {
                // Order the workers to gather so they get a stable state without transitioning to PlayerGuard
                for (auto &[worker, patch] : workersAndPatches)
                {
                    worker->gather(patch);
                }
            }
            else if (BWAPI::Broodwar->getFrameCount() > 1500 && BWAPI::Broodwar->getFrameCount() < 2950)
            {
                // Execute the planned resends and log the actual order process timer values
                for (auto &[worker, patch] : workersAndPatches)
                {
                    if (gatherCommandFrames.contains(currentFrame))
                    {
                        CherryVis::log(worker->getID()) << "Sent gather command";
                        worker->gather(patch);
                    }
                    if (returnCommandFrames.contains(currentFrame))
                    {
                        CherryVis::log(worker->getID()) << "Sent return command";
                        worker->returnCargo();
                    }

                    workerToFrameToActualOrderProcessTimer[worker][currentFrame] = worker->getOrderProcessTimer();

                    CherryVis::log(worker->getID()) << "Actual: " << worker->getOrderProcessTimer();
                }
            }
            else if (BWAPI::Broodwar->getFrameCount() == 2950)
            {
                // Perform validation before the game ends to ensure we have the expected alignment between currentFrame and getFrameCount()

                // Start by computing a set of frames where the worker order timers differ
                // This is useful for differentiating frames when we should be able to predict their value exactly from frames where the order
                // timers have been scrambled
                std::set<int> framesWithDifferingOrderTimers;
                for (int f = 1500; f < 2900; f++)
                {
                    int value = -1;
                    bool equal = true;
                    for (auto &[_, frameToActualOrderProcessTimer] : workerToFrameToActualOrderProcessTimer)
                    {
                        if (value == -1) value = frameToActualOrderProcessTimer[f];
                        if (frameToActualOrderProcessTimer[f] != value) equal = false;
                    }
                    if (!equal) framesWithDifferingOrderTimers.insert(f);
                }

                for (auto &[worker, _] : workersAndPatches)
                {
                    auto &frameToActualOrderProcessTimer = workerToFrameToActualOrderProcessTimer[worker];

                    auto validate = [&](
                            int startFrame, int endFrame, bool useKnownInitialValue, int expectedPossibleValues)
                    {
                        // Validate that the expected number of possible values makes sense against the frames where we know the order timers differ
                        if (useKnownInitialValue)
                        {
                            // If the end frame isn't a reset frame, and the order timers where the same at the end of the previous frame, we should
                            // be expecting to know the result
                            if (!OrderProcessTimer::isResetFrame(endFrame) && !framesWithDifferingOrderTimers.contains(endFrame - 1))
                            {
                                EXPECT_EQ(1, expectedPossibleValues);
                            }

                            // If the order timers were different at both the end of the end frame and the frame before, we should not be expecting
                            // to know the result
                            if (framesWithDifferingOrderTimers.contains(endFrame) && framesWithDifferingOrderTimers.contains(endFrame - 1))
                            {
                                EXPECT_GE(8, expectedPossibleValues);
                            }
                        }

                        // Utility to get the value of the order process timer at the start of the given frame
                        // If there was a resend taking effect on the frame, this is handled appropriately
                        // If the frame is a reset frame, returns the value that was reset to prior to the frame processing
                        auto actualOrderProcessTimerAtStartOfFrame = [&](int frame)
                        {
                            // A resend taking effect trumps any reset frame
                            if (gatherCommandFrames.contains(frame - BWAPI::Broodwar->getLatencyFrames() - 1)
                                || returnCommandFrames.contains(frame - BWAPI::Broodwar->getLatencyFrames() - 1))
                            {
                                return 0;
                            }

                            // If there isn't a reset, the value at the start of the frame is equivalent to the value at the end of the previous one
                            if (!OrderProcessTimer::isResetFrame(frame)) return frameToActualOrderProcessTimer[frame - 1];

                            // Figure out the value the order process timer was reset to by taking the value at the end of the frame and running the
                            // cycle backwards
                            if (frameToActualOrderProcessTimer[frame] == 8) return 0;
                            return frameToActualOrderProcessTimer[frame] + 1;
                        };

                        std::multiset<int> initialValues;
                        if (useKnownInitialValue)
                        {
                            initialValues = {actualOrderProcessTimerAtStartOfFrame(startFrame)};
                        }
                        else
                        {
                            initialValues = {0, 1, 2, 3, 4, 5, 6, 7, 8};
                        }

                        auto expected = actualOrderProcessTimerAtStartOfFrame(endFrame);

                        auto actual = OrderProcessTimer::atStartOfFrameAtDelta(
                                startFrame, initialValues, gatherCommandFrames, returnCommandFrames, endFrame - startFrame);
                        if (!actual.contains(expected) || actual.size() != expectedPossibleValues)
                        {
                            std::ostringstream values;
                            values << "[";
                            std::string sep;
                            for (auto value : actual)
                            {
                                values << sep << value;
                                sep = ",";
                            }
                            values << "]";

                            EXPECT_TRUE(actual.contains(expected))
                                << "worker " << worker->getID() << " fails with " << startFrame << "-" << endFrame
                                << "; expected value " << expected << " not in " << values.str();
                            EXPECT_EQ(expectedPossibleValues, actual.size())
                                << "worker " << worker->getID() << " fails with " << startFrame << "-" << endFrame
                                << "; expected " << expectedPossibleValues << " values, got " << values.str();
                        }
                    };

                    for (int i = 0; i < 10; i++)
                    {
                        for (int j = 1; j <= 10; j++)
                        {
                            // No resend or reset
                            validate(1520 + i, 1520 + i + j, true, 1);

                            auto expectedPossibleValues = [&](int frame, int startValues)
                            {
                                int startFrame = frame + i;
                                int endFrame = startFrame + j;

                                int resendFrameTakesEffect = -1;
                                bool resendIsGather = false;
                                int resetFrame = -1;
                                for (int f = startFrame; f <= endFrame; f++)
                                {
                                    if (OrderProcessTimer::isResetFrame(f)) resetFrame = f;
                                    if (gatherCommandFrames.contains(f - BWAPI::Broodwar->getLatencyFrames() - 1))
                                    {
                                        resendFrameTakesEffect = f;
                                        resendIsGather = true;
                                    }
                                    if (returnCommandFrames.contains(f - BWAPI::Broodwar->getLatencyFrames() - 1))
                                    {
                                        resendFrameTakesEffect = f;
                                    }
                                }

                                // If there is a resend frame, there will be a known order process timer value after it takes effect
                                // However the order process timer value will become unknown again if there is a reset afterwards
                                if (resendFrameTakesEffect != -1 && (resetFrame == -1 || resetFrame <= resendFrameTakesEffect))
                                {
                                    if (resendFrameTakesEffect < startFrame) return startValues;
                                    if (!resendIsGather && resendFrameTakesEffect == startFrame) return startValues;

                                    return 1;
                                }

                                // If there is a reset frame, there will be 8 possible values after it kicks in
                                if (resetFrame != -1)
                                {
                                    if (resetFrame <= startFrame || resetFrame > endFrame) return startValues;
                                    return 8;
                                }

                                return startValues;
                            };

                            // Resends with no reset
                            validate(1548 + i, 1548 + i + j, true, 1);
                            validate(1548 + i, 1548 + i + j, false, expectedPossibleValues(1548, 9));
                            validate(2148 + i, 2148 + i + j, true, 1);
                            validate(2148 + i, 2148 + i + j, false, expectedPossibleValues(2148, 9));

                            // Reset with no resends
                            validate(2705 + i, 2705 + i + j, true, expectedPossibleValues(2705, 1));
                            validate(2705 + i, 2705 + i + j, false, expectedPossibleValues(2705, 9));

                            // Resends with reset
                            validate(1501 + i, 1501 + i + j, true, expectedPossibleValues(1501, 1));
                            validate(1501 + i, 1501 + i + j, false, expectedPossibleValues(1501, 9));
                            validate(1651 + i, 1651 + i + j, true, expectedPossibleValues(1651, 1));
                            validate(1651 + i, 1651 + i + j, false, expectedPossibleValues(1651, 9));
                            validate(1801 + i, 1801 + i + j, true, expectedPossibleValues(1801, 1));
                            validate(1801 + i, 1801 + i + j, false, expectedPossibleValues(1801, 9));
                            validate(1951 + i, 1951 + i + j, true, expectedPossibleValues(1951, 1));
                            validate(1951 + i, 1951 + i + j, false, expectedPossibleValues(1951, 9));
                            validate(2101 + i, 2101 + i + j, true, expectedPossibleValues(2101, 1));
                            validate(2101 + i, 2101 + i + j, false, expectedPossibleValues(2101, 9));
                            validate(2251 + i, 2251 + i + j, true, expectedPossibleValues(2251, 1));
                            validate(2251 + i, 2251 + i + j, false, expectedPossibleValues(2251, 9));
                            validate(2401 + i, 2401 + i + j, true, expectedPossibleValues(2401, 1));
                            validate(2401 + i, 2401 + i + j, false, expectedPossibleValues(2401, 9));
                            validate(2551 + i, 2551 + i + j, true, expectedPossibleValues(2551, 1));
                            validate(2551 + i, 2551 + i + j, false, expectedPossibleValues(2551, 9));
                        }
                    }
                }
            }

            InstrumentedDoNothingModule::onFrameEnd();
        }
    };
}

TEST(OrderProcessTimer, AtStartOfFrameAtDeltaTests)
{
    BWTest test;
    AtStartOfFrameAtDeltaModule module;
    test.myModule = [&]()
    {
        return &module;
    };
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.map = Maps::GetOne("Tau Cross");
    test.randomSeed = 42;
    test.frameLimit = 3000;
    test.expectWin = false;
    test.run();
}

TEST(OrderProcessTimer, StaticTestCases)
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

        // Set the bot frame to the frame before the engine frame, such as we do when running the bot for real
        // See the comment in OrderProcessTimer for details
        currentFrame = 4;

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
