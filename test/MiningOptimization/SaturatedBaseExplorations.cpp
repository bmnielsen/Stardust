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
    test.frameLimit = 10000;
    test.expectWin = false;

    std::vector<BWAPI::Unit> otherPatches;
    BWAPI::Unit targetPatch;

    std::vector<std::pair<BWAPI::Unit, BWAPI::Unit>> otherWorkersAndPatch;
    std::vector<BWAPI::Unit> testWorkers;

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
                                        BWAPI::Position(240, 296 + 32));
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                        BWAPI::UnitTypes::Protoss_Probe,
                                        BWAPI::Position(240 + 24, 296 + 32));
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                        BWAPI::UnitTypes::Protoss_Probe,
                                        BWAPI::Position(240 + 48, 296 + 32));
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                        BWAPI::UnitTypes::Protoss_Probe,
                                        BWAPI::Position(240 + 72, 296 + 32));
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
            }
        }

        if (BWAPI::Broodwar->getFrameCount() > 20 &&
            BWAPI::Broodwar->getFrameCount() % 80 == 0)
        {
            for (auto &[worker, patch] : otherWorkersAndPatch)
            {
                worker->gather(patch);
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
    };
    test.run();
}
