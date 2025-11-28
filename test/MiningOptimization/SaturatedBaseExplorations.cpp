//
// This test is created to experiment with worker timings when a base is completely saturated, i.e. all patches are currently being mined.
//

#include "BWTest.h"
#include "DoNothingModule.h"

TEST(SaturatedBase, SinglePatch)
{
    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    test.randomSeed = 42;
    test.opponentRace = BWAPI::Races::Terran;
    test.opponentModule = test.myModule = []()
    {
        return new DoNothingModule();
    };
    test.frameLimit = 1000;
    test.expectWin = false;

    BWAPI::Unit targetPatch;
    BWAPI::Unit otherPatch;

    int stopOtherWorkersMiningFrame = 190;
    int stopMiningWorkerFrame = -1;
    int resendApproachingWorkerFrame = 178;

//    std::vector<std::pair<BWAPI::Unit, BWAPI::Unit>> otherWorkersAndPatch;
//    std::vector<BWAPI::Unit> testWorkers;
//
////    int stopFrame = 283; // Frame when all other patch workers are ordered to stop
//    int resendOnApproachFrame = 261; // Frame when an approaching worker is ordered to gather the target patch again
//    int resendWhileMiningFrame = 220; // Frame when an approaching worker is ordered to gather the target patch again

    test.onFrameMine = [&]()
    {
        if (BWAPI::Broodwar->getFrameCount() == 10)
        {
            for (auto &patch : BWAPI::Broodwar->getMinerals())
            {
                if (patch->getDistance(BWAPI::Position(288, 40)) > 400) continue;
                if (patch->getTilePosition() == BWAPI::TilePosition(2, 8))
                {
                    targetPatch = patch;
                }
                else if (patch->getTilePosition() == BWAPI::TilePosition(3, 12))
                {
                    otherPatch = patch;
                }
                else
                {
                    BWAPI::Broodwar->killUnit(patch);
                }
            }

            for (auto &worker : BWAPI::Broodwar->self()->getUnits())
            {
                if (worker->getType().isWorker()) BWAPI::Broodwar->killUnit(worker);
            }
        }

        if (BWAPI::Broodwar->getFrameCount() == 15)
        {
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                        BWAPI::UnitTypes::Protoss_Probe,
                                        targetPatch->getPosition() + BWAPI::Position(64, 0));
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                        BWAPI::UnitTypes::Protoss_Probe,
                                        targetPatch->getPosition() + BWAPI::Position(96, 0));
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                        BWAPI::UnitTypes::Protoss_Probe,
                                        otherPatch->getPosition() + BWAPI::Position(64, 0));
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                        BWAPI::UnitTypes::Protoss_Probe,
                                        otherPatch->getPosition() + BWAPI::Position(96, 0));
        }

        if (BWAPI::Broodwar->getFrameCount() == 20)
        {
            for (auto &worker : BWAPI::Broodwar->self()->getUnits())
            {
                if (worker->getDistance(otherPatch) < 64)
                {
                    worker->gather(otherPatch);
                }
                else
                {
                    worker->gather(targetPatch);
                }
            }
        }

        if (BWAPI::Broodwar->getFrameCount() > 20)
        {
            for (auto &worker : BWAPI::Broodwar->self()->getUnits())
            {
                if (worker->getDistance(otherPatch) < 64)
                {
                    if (BWAPI::Broodwar->getFrameCount() >= stopOtherWorkersMiningFrame)
                    {
                        worker->stop();
                    }
                    else
                    {
                        if (worker->getOrder() == BWAPI::Orders::MiningMinerals && worker->getOrderTimer() == 40)
                        {
                            worker->stop();
                        }
                        if (worker->getOrder() == BWAPI::Orders::PlayerGuard)
                        {
                            worker->gather(otherPatch);
                        }
                    }
                }
                else if (BWAPI::Broodwar->getFrameCount() == stopMiningWorkerFrame && worker->getOrder() == BWAPI::Orders::MiningMinerals)
                {
                    worker->stop();
                }
                else if (BWAPI::Broodwar->getFrameCount() == resendApproachingWorkerFrame && worker->getOrder() == BWAPI::Orders::MoveToMinerals)
                {
                    worker->gather(targetPatch);
                }
                else if (BWAPI::Broodwar->getFrameCount() == (resendApproachingWorkerFrame - 15) && worker->getOrder() == BWAPI::Orders::MiningMinerals)
                {
                    worker->gather(targetPatch);
                }
            }
        }

//        if (BWAPI::Broodwar->getFrameCount() > 20)
//        {
//            for (auto &[worker, patch] : otherWorkersAndPatch)
//            {
//                if (worker->getOrder() == BWAPI::Orders::MiningMinerals && worker->getOrderTimer() == 40)
//                {
//                    worker->stop();
//                }
//                if ((worker->getOrder() == BWAPI::Orders::MoveToMinerals && worker->getOrderTarget() != patch)
//                    || worker->getOrder() == BWAPI::Orders::PlayerGuard)
//                {
//                    worker->gather(patch);
//                }
//            }
//        }
//
//        if (BWAPI::Broodwar->getFrameCount() == 50)
//        {
//            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
//                                        BWAPI::UnitTypes::Protoss_Probe,
//                                        BWAPI::Position(240 + 48, 296 + 32));
//            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
//                                        BWAPI::UnitTypes::Protoss_Probe,
//                                        BWAPI::Position(240 + 72, 296 + 32));
//        }
//
//        if (BWAPI::Broodwar->getFrameCount() == 60)
//        {
//            for (auto &worker : BWAPI::Broodwar->self()->getUnits())
//            {
//                if (!worker->getType().isWorker()) continue;
//                if (worker->getLastCommand().getType() != BWAPI::UnitCommandTypes::None) continue;
//                testWorkers.emplace_back(worker);
//                worker->gather(targetPatch);
//            }
//        }
//
//        if (BWAPI::Broodwar->getFrameCount() == resendOnApproachFrame)
//        {
//            for (auto &worker : testWorkers)
//            {
//                if (worker->getOrder() != BWAPI::Orders::MoveToMinerals) continue;
//                worker->gather(targetPatch);
//            }
//        }
//
//        if (BWAPI::Broodwar->getFrameCount() == resendWhileMiningFrame)
//        {
//            for (auto &worker : testWorkers)
//            {
//                if (worker->getOrder() != BWAPI::Orders::MiningMinerals) continue;
//                worker->gather(targetPatch);
//            }
//        }
//
//        if (BWAPI::Broodwar->getFrameCount() > 60)
//        {
//            for (auto &worker : testWorkers)
//            {
////                if (BWAPI::Broodwar->getFrameCount() == 277)
////                {
////                    std::cout << worker->getOrder() << " at dist " << worker->getDistance(worker->getOrderTarget()) << " timer " << worker->getOrderProcessTimer()
////                }
//
//                if (worker->getOrder() != BWAPI::Orders::MoveToMinerals) continue;
//                if (worker->getOrderProcessTimer() != 8) continue;
//                if (worker->getDistance(worker->getOrderTarget()) > 10) continue;
//                if (worker->getDistance(worker->getOrderTarget()) == 0) continue;
//
//                std::cout << BWAPI::Broodwar->getFrameCount() << ": Worker " << worker->getID() << " approaching at order timer 0" << std::endl;
//            }
//        }
//
//        if (BWAPI::Broodwar->getFrameCount() == stopFrame)
//        {
//            for (auto &[worker, _] : otherWorkersAndPatch)
//            {
//                worker->stop();
//            }
//        }
    };
    test.run();
}
