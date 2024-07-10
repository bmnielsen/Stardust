#include "BWTest.h"

#include "DoNothingModule.h"
#include "InstrumentedDoNothingModule.h"
#include "WorkerMiningInstrumentation.h"

namespace
{
    std::map<Resource, std::set<MyWorker>> mineralPatchWorkers;

    struct TestResultData
    {
        int firstWorkerStartedMining = 0;
        int firstWorkerFinishedMining = 0;
        int secondWorkerStartedMining = 0;

        [[nodiscard]] int miningTime() const
        {
            return firstWorkerFinishedMining - firstWorkerStartedMining;
        }

        [[nodiscard]] int wastedTime() const
        {
            return secondWorkerStartedMining - firstWorkerFinishedMining - 1;
        }
    };

    BWAPI::Unit getPatchUnit(BWAPI::TilePosition patchTile)
    {
        if (patchTile == BWAPI::TilePositions::Invalid) return nullptr;

        for (auto &unit : BWAPI::Broodwar->getStaticNeutralUnits())
        {
            if (unit->getInitialTilePosition() == patchTile) return unit;
        }

        return nullptr;
    }

    class OptimizeWorkerTakeoverModule : public InstrumentedDoNothingModule
    {
    private:
        // Time before an order timer reset the first worker should start mining
        int startMiningDelta;

        // Results of the test
        std::shared_ptr<TestResultData> result;

        // States:
        // 0 - identifies the workers needed for the test
        // 1 - picks the two workers for the test and orders them to gather each frame until the right frame is reached
        // 2,3 - allows the first worker to mine, orders the second worker to mine at the appropriate time and goes to next state
        // 4,5,6 - logs unit behaviour until second worker starts mining, then quits
        int state;

        BWAPI::Unit firstWorker;
        BWAPI::Unit secondWorker;

    public:
        explicit OptimizeWorkerTakeoverModule(int startMiningDelta, std::shared_ptr<TestResultData> result)
                : InstrumentedDoNothingModule(true)
                , startMiningDelta(startMiningDelta)
                , result(std::move(result))
                , state(0)
                , firstWorker(nullptr)
                , secondWorker(nullptr)
        {}

        void onStart() override
        {
            InstrumentedDoNothingModule::onStart();

            auto getPatchAndWorkers = [&]() -> std::map<Resource, std::set<MyWorker>>&
            {
                mineralPatchWorkers.clear();

                auto first = std::dynamic_pointer_cast<MyWorkerImpl>(Units::get(firstWorker));
                auto second = std::dynamic_pointer_cast<MyWorkerImpl>(Units::get(secondWorker));
                auto patch = Units::resourceAt(BWAPI::TilePosition(2, 118));
                if (!first || !second || !patch) return mineralPatchWorkers;

                mineralPatchWorkers[patch] = {first, second};
                return mineralPatchWorkers;
            };

            WorkerMiningInstrumentation::initialize(getPatchAndWorkers);
        }

        void onFrame() override
        {
            InstrumentedDoNothingModule::onFrameStart();
            WorkerMiningInstrumentation::update();

            CherryVis::setBoardValue("state-start", (std::ostringstream() << state).str());

            auto patch = getPatchUnit(BWAPI::TilePosition(2, 118));
            if (!patch)
            {
                Log::Get() << "Unable to get patch";
                BWAPI::Broodwar->leaveGame();
            }

            int framesSinceLastOrderTimerReset = (currentFrame - 8) % 150;
            int framesToNextOrderTimerReset = 150 - framesSinceLastOrderTimerReset;

            CherryVis::log() << "Last reset ago: " << framesSinceLastOrderTimerReset
                << "\nNext reset in: " << framesToNextOrderTimerReset;

            auto issueGatherCommand = [&patch](BWAPI::Unit worker)
            {
                if (!worker->gather(patch))
                {
                    CherryVis::log() << "Unable to issue gather: " << BWAPI::Broodwar->getLastError();
                    Log::Get() << "Unable to issue gather: " << BWAPI::Broodwar->getLastError();
                }
            };

            switch (state)
            {
                case 0:
                {
                    // First worker is a unit that we know will reset to 8
                    // Second worker doesn't matter
                    for (auto unit : BWAPI::Broodwar->self()->getUnits())
                    {
                        if (!unit->getType().isWorker()) continue;

                        if (unit->getPosition().x == 288)
                        {
                            firstWorker = unit;
                        }
                        else if (unit->getPosition().x == 264)
                        {
                            secondWorker = unit;
                        }
                    }

                    issueGatherCommand(firstWorker);
                    issueGatherCommand(secondWorker);

                    state = 1;
                    break;
                }

                case 1:
                {
                    // Compute the frame delta to where we want to issue the final gather command to the first worker
                    int framesToFinalGatherCommand = framesToNextOrderTimerReset - 11 - BWAPI::Broodwar->getLatencyFrames() - startMiningDelta;

                    // Issue gather commands at regular intervals to avoid the workers switching patches or starting mining
                    if (framesToFinalGatherCommand % 8 == 0)
                    {
                        issueGatherCommand(firstWorker);
                        issueGatherCommand(secondWorker);
                    }

                    // Switch state when the first worker is at the patch and should be allowed to start mining
                    if (firstWorker->getDistance(patch) == 0 && framesToFinalGatherCommand == 0)
                    {
                        CherryVis::log() << "Allowing first worker to start mining";
                        Log::Get() << "Allowing first worker to start mining";
                        state = 2;
                    }
                    
                    break;
                }

                case 2:
                case 3:
                {
                    // Wait until the first worker starts mining, issuing gather commands regularly for the second worker
                    if (firstWorker->getOrder() != BWAPI::Orders::MiningMinerals)
                    {
                        if ((currentFrame - secondWorker->getLastCommandFrame()) > 3)
                        {
                            issueGatherCommand(secondWorker);
                        }
                        break;
                    }

                    if (state == 2)
                    {
                        result->firstWorkerStartedMining = currentFrame;

                        CherryVis::log() << "First worker started mining";
                        Log::Get() << "First worker started mining";
                        state = 3;
                    }

                    // Sometimes the first worker will wait an extra cycle before starting the mining timer for an unknown reason
                    if (firstWorker->getOrderTimer() > 0)
                    {
                        int actualStartFrame = currentFrame - (75 - firstWorker->getOrderTimer()) - 1;
                        if (result->firstWorkerStartedMining != actualStartFrame)
                        {
                            CherryVis::log() << "Correcting actual worker start frame: was=" << result->firstWorkerStartedMining
                                << ", now=" << actualStartFrame;
                            Log::Get() << "Correcting actual worker start frame: was=" << result->firstWorkerStartedMining
                                       << ", now=" << actualStartFrame;
                            result->firstWorkerStartedMining = actualStartFrame;
                        }
                    }

                    // TODO: Figure out why sometimes the second worker needs to wait an extra frame

                    // Without order timer resets, we can take over 82 frames after the first worker started mining
                    int takeOverFrame = result->firstWorkerStartedMining + 82;

                    // Compute the frame of the order timer reset prior to the take over frame
                    int previousOrderTimerReset = takeOverFrame - ((takeOverFrame - 8) % 150);
                    if (previousOrderTimerReset == takeOverFrame) previousOrderTimerReset -= 150;

                    // If the order timer reset during mining, adjust our take over frame
                    // We always assume the worst-case scenario (needing to wait a full cycle after the mining timer expires)
                    if (previousOrderTimerReset > result->firstWorkerStartedMining)
                    {
                        CherryVis::log() << "Take over frame adjusted from " << takeOverFrame
                            << " to " << std::max(result->firstWorkerStartedMining + 84, previousOrderTimerReset + 8)
                            << "; previousOrderTimerReset=" << previousOrderTimerReset
                            << "; firstWorkerStartedMining=" << result->firstWorkerStartedMining;
                        takeOverFrame = std::max(result->firstWorkerStartedMining + 84, previousOrderTimerReset + 8);
                    }

                    CherryVis::setBoardValue("take-over-frame", (std::ostringstream() << takeOverFrame).str());

                    // Compute the frame we need to send a command for it to take effect on the takeover frame and on the order timer reset frame
                    int commandFrameForTakeOver = takeOverFrame - 11 - BWAPI::Broodwar->getLatencyFrames();
                    int commandFrameForReset = previousOrderTimerReset - BWAPI::Broodwar->getLatencyFrames();

                    // If the takeover frame comes first, delay sending the order so it takes effect when the order timer resets instead
                    // This is to avoid situations where the second worker's command takes effect too soon, causing it to switch to a different patch
                    if (commandFrameForReset > commandFrameForTakeOver)
                    {
                        commandFrameForTakeOver = commandFrameForReset;
                    }

                    // Compute the number of frames until the next command we have to send
                    // We send regular commands to avoid having the worker switch patches
                    int framesToNextCommand;
                    if (currentFrame <= commandFrameForReset && (commandFrameForTakeOver - commandFrameForReset) > 3)
                    {
                        framesToNextCommand = std::min(commandFrameForReset, commandFrameForTakeOver) - currentFrame;
                    }
                    else
                    {
                        framesToNextCommand = commandFrameForTakeOver - currentFrame;
                    }

                    if (framesToNextCommand % 4 == 0)
                    {
                        issueGatherCommand(secondWorker);

                        if (currentFrame == commandFrameForTakeOver)
                        {
                            CherryVis::log() << "Allowing second worker to start mining";
                            Log::Get() << "Allowing second worker to start mining";
                            state = 4;
                        }
                    }

                    break;
                }

                case 4:
                {
                    if (firstWorker->isCarryingMinerals())
                    {
                        result->firstWorkerFinishedMining = currentFrame;

                        CherryVis::log() << "First worker finished mining";
                        Log::Get() << "First worker finished mining";
                        state = 5;
                    }

                    break;
                }

                case 5:
                {
                    if (secondWorker->getOrder() == BWAPI::Orders::MiningMinerals)
                    {
                        result->secondWorkerStartedMining = currentFrame;

                        CherryVis::log() << "Second worker started mining";
                        Log::Get() << "Second worker started mining";
                        state = 6;
                    }

                    break;
                }

                case 6:
                {
                    if (secondWorker->getOrderTimer() == 70)
                    {
                        BWAPI::Broodwar->leaveGame();
                    }

                    break;
                }
            }
            
            auto outputWorkerData = [](const BWAPI::Unit &worker)
            {
                if (!worker) return;

                std::ostringstream debug;

                // First line is command
                debug << "cmd=" << worker->getLastCommand().getType() << ";f="
                      << (currentFrame - worker->getLastCommandFrame());
                if (worker->getLastCommand().getTarget())
                {
                    debug << ";tgt=" << worker->getLastCommand().getTarget()->getType()
                          << "#" << worker->getLastCommand().getTarget()->getID()
                          << "@" << BWAPI::WalkPosition(worker->getLastCommand().getTarget()->getPosition())
                          << ";d=" << worker->getLastCommand().getTarget()->getDistance(worker);
                }
                else if (worker->getLastCommand().getTargetPosition())
                {
                    debug << ";tgt=" << BWAPI::WalkPosition(worker->getLastCommand().getTargetPosition());
                }

                // Next line is order
                debug << "\nord=" << worker->getOrder() << ";t=" << worker->getOrderTimer();
                if (worker->getOrderTarget())
                {
                    debug << ";tgt=" << worker->getOrderTarget()->getType()
                          << "#" << worker->getOrderTarget()->getID()
                          << "@" << BWAPI::WalkPosition(worker->getOrderTarget()->getPosition())
                          << ";d=" << worker->getOrderTarget()->getDistance(worker);
                }
                else if (worker->getOrderTargetPosition())
                {
                    debug << ";tgt=" << BWAPI::WalkPosition(worker->getOrderTargetPosition());
                }

                // Next line is worker-specific
                debug << "\ncarrying=" << worker->isCarryingMinerals();
                
                CherryVis::log(worker->getID()) << debug.str();
            };
            outputWorkerData(firstWorker);
            outputWorkerData(secondWorker);

            WorkerMiningInstrumentation::writeInstrumentation();

            CherryVis::setBoardValue("state-end", (std::ostringstream() << state).str());
            InstrumentedDoNothingModule::onFrameEnd();
        }
    };

    std::shared_ptr<TestResultData> runWithDelta(int delta)
    {
        auto result = std::make_shared<TestResultData>();

        BWTest test;
        test.map = Maps::GetOne("Fighting");
        test.opponentRace = BWAPI::Races::Protoss;
        test.randomSeed = 43875;
        test.frameLimit = 1000;
        test.expectWin = false;
        test.opponentModule = []()
        {
            return new DoNothingModule();
        };
        test.myModule = [&]()
        {
            return new OptimizeWorkerTakeoverModule(delta, result);
        };

        std::ostringstream replayName;
        replayName << ::testing::UnitTest::GetInstance()->current_test_info()->test_case_name();
        replayName << "_" << ::testing::UnitTest::GetInstance()->current_test_info()->name();
        replayName << "_delta=" << delta;
        test.replayName = replayName.str();

        test.run();

        std::cout << "Result for delta " << delta << ": Mining time = " << result->miningTime() << "; Wastage = " << result->wastedTime()
                  << std::endl;

        return result;
    }

    void runWithRange(int start, int end)
    {
        int totalWastage = 0;
        for (int delta = start; delta <= end; delta++)
        {
            totalWastage += runWithDelta(delta)->wastedTime();
        }

        std::cout << std::fixed << std::showpoint << std::setprecision(4)
                  << "Total wastage: " << totalWastage << " over " << (end - start + 1) << " tests; "
                  << ((double)totalWastage / (double)(end - start + 1)) << " per test" << std::endl;
    }
}

TEST(OptimizeWorkerTakeover, NoOrderTimerReset)
{
    runWithDelta(120);
}

TEST(OptimizeWorkerTakeover, WorstCaseOrderTimerResetDuringMiningTimer)
{
    runWithDelta(14);
}

TEST(OptimizeWorkerTakeover, UnitBusyCase)
{
    runWithDelta(71);
}

TEST(OptimizeWorkerTakeover, WorstCaseOrderTimerResetDuringCommandWindow)
{
    runWithDelta(77);
}

TEST(OptimizeWorkerTakeover, WorstCaseOrderTimerResetAfterMiningTimer)
{
    runWithDelta(82);
}

TEST(OptimizeWorkerTakeover, OrderTimerResetExactlyAfterMining)
{
    runWithDelta(83);
}

TEST(OptimizeWorkerTakeover, MultipleResetDuringMining)
{
    runWithRange(10, 58);
}

TEST(OptimizeWorkerTakeover, MultipleResetAroundMiningCompletion)
{
    runWithRange(60, 87);
}

TEST(OptimizeWorkerTakeover, MultipleFullSpectrum)
{
    runWithRange(0, 132);
}
