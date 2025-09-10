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

    std::vector<BWAPI::Unit> otherPatches;
    BWAPI::Unit targetPatch;

    std::vector<std::pair<BWAPI::Unit, BWAPI::Unit>> otherWorkersAndPatch;
    std::vector<BWAPI::Unit> testWorkers;

    int stopFrame = 283; // Frame when all other patch workers are ordered to stop
    int resendOnApproachFrame = 261; // Frame when an approaching worker is ordered to gather the target patch again
    int resendWhileMiningFrame = 220; // Frame when an approaching worker is ordered to gather the target patch again

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
                else
                {
                    otherPatches.push_back(patch);
                }
            }

            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                        BWAPI::UnitTypes::Protoss_Probe,
                                        BWAPI::Position(240 + 24 * 4, 296));
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                        BWAPI::UnitTypes::Protoss_Probe,
                                        BWAPI::Position(240 + 24 * 5, 296));
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                        BWAPI::UnitTypes::Protoss_Probe,
                                        BWAPI::Position(240 + 24 * 6, 296));
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                        BWAPI::UnitTypes::Protoss_Probe,
                                        BWAPI::Position(240 + 24 * 7, 296));
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                        BWAPI::UnitTypes::Protoss_Probe,
                                        BWAPI::Position(240 + 24 * 0, 296 + 32));
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                        BWAPI::UnitTypes::Protoss_Probe,
                                        BWAPI::Position(240 + 24 * 1, 296 + 32));
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                        BWAPI::UnitTypes::Protoss_Probe,
                                        BWAPI::Position(240 + 24 * 2, 296 + 32));
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                        BWAPI::UnitTypes::Protoss_Probe,
                                        BWAPI::Position(240 + 24 * 3, 296 + 32));
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                        BWAPI::UnitTypes::Protoss_Probe,
                                        BWAPI::Position(240 + 24 * 4, 296 + 32));
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                        BWAPI::UnitTypes::Protoss_Probe,
                                        BWAPI::Position(240 + 24 * 5, 296 + 32));
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                        BWAPI::UnitTypes::Protoss_Probe,
                                        BWAPI::Position(240 + 24 * 6, 296 + 32));
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                        BWAPI::UnitTypes::Protoss_Probe,
                                        BWAPI::Position(240 + 24 * 7, 296 + 32));
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                        BWAPI::UnitTypes::Protoss_Probe,
                                        BWAPI::Position(240 + 24 * 0, 296 + 64));
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                        BWAPI::UnitTypes::Protoss_Probe,
                                        BWAPI::Position(240 + 24 * 1, 296 + 64));
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                        BWAPI::UnitTypes::Protoss_Probe,
                                        BWAPI::Position(240 + 24 * 2, 296 + 64));
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                        BWAPI::UnitTypes::Protoss_Probe,
                                        BWAPI::Position(240 + 24 * 3, 296 + 64));
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                        BWAPI::UnitTypes::Protoss_Probe,
                                        BWAPI::Position(240 + 24 * 4, 296 + 64));
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                        BWAPI::UnitTypes::Protoss_Probe,
                                        BWAPI::Position(240 + 24 * 5, 296 + 64));
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                        BWAPI::UnitTypes::Protoss_Probe,
                                        BWAPI::Position(240 + 24 * 6, 296 + 64));
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                        BWAPI::UnitTypes::Protoss_Probe,
                                        BWAPI::Position(240 + 24 * 7, 296 + 64));
        }

        if (BWAPI::Broodwar->getFrameCount() == 20)
        {
            auto patchIt = otherPatches.begin();
            for (auto &worker : BWAPI::Broodwar->self()->getUnits())
            {
                if (!worker->getType().isWorker()) continue;
                worker->gather(*patchIt);
                otherWorkersAndPatch.emplace_back(worker, *patchIt);
                patchIt++;
                if (patchIt == otherPatches.end()) patchIt = otherPatches.begin();
            }
        }

        if (BWAPI::Broodwar->getFrameCount() > 20 && BWAPI::Broodwar->getFrameCount() < stopFrame)
        {
            for (auto &[worker, patch] : otherWorkersAndPatch)
            {
                if (!worker->isCarryingMinerals() && worker->getOrderTarget() != patch)
                {
                    worker->gather(patch);
                }
            }
        }

        if (BWAPI::Broodwar->getFrameCount() == 50)
        {
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                        BWAPI::UnitTypes::Protoss_Probe,
                                        BWAPI::Position(240 + 48, 296 + 32));
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                        BWAPI::UnitTypes::Protoss_Probe,
                                        BWAPI::Position(240 + 72, 296 + 32));
        }

        if (BWAPI::Broodwar->getFrameCount() == 60)
        {
            for (auto &worker : BWAPI::Broodwar->self()->getUnits())
            {
                if (!worker->getType().isWorker()) continue;
                if (worker->getLastCommand().getType() == BWAPI::UnitCommandTypes::Gather) continue;
                testWorkers.emplace_back(worker);
                worker->gather(targetPatch);
            }
        }

        if (BWAPI::Broodwar->getFrameCount() == resendOnApproachFrame)
        {
            for (auto &worker : testWorkers)
            {
                if (worker->getOrder() != BWAPI::Orders::MoveToMinerals) continue;
                worker->gather(targetPatch);
            }
        }

        if (BWAPI::Broodwar->getFrameCount() == resendWhileMiningFrame)
        {
            for (auto &worker : testWorkers)
            {
                if (worker->getOrder() != BWAPI::Orders::MiningMinerals) continue;
                worker->gather(targetPatch);
            }
        }

        if (BWAPI::Broodwar->getFrameCount() > 60)
        {
            for (auto &worker : testWorkers)
            {
//                if (BWAPI::Broodwar->getFrameCount() == 277)
//                {
//                    std::cout << worker->getOrder() << " at dist " << worker->getDistance(worker->getOrderTarget()) << " timer " << worker->getOrderProcessTimer()
//                }

                if (worker->getOrder() != BWAPI::Orders::MoveToMinerals) continue;
                if (worker->getOrderProcessTimer() != 8) continue;
                if (worker->getDistance(worker->getOrderTarget()) > 10) continue;
                if (worker->getDistance(worker->getOrderTarget()) == 0) continue;

                std::cout << BWAPI::Broodwar->getFrameCount() << ": Worker " << worker->getID() << " approaching at order timer 0" << std::endl;
            }
        }

        if (BWAPI::Broodwar->getFrameCount() == stopFrame)
        {
            for (auto &[worker, _] : otherWorkersAndPatch)
            {
                worker->stop();
            }
        }
    };
    test.run();
}
