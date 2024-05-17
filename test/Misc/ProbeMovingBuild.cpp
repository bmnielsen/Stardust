#include "BWTest.h"

#include "DoNothingModule.h"

TEST(ProbeMovingBuild, ProbeMovingBuild)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.myModule = []()
    {
        return new DoNothingModule();
    };
    test.myRace = BWAPI::Races::Zerg;
    test.opponentRace = BWAPI::Races::Zerg;
    test.map = Maps::GetOne("Fighting Spirit");
    test.randomSeed = 42;
    test.frameLimit = 1000;
    test.expectWin = false;

    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(115, 15))),
    };

    test.onFrameMine = []()
    {
        if (BWAPI::Broodwar->getFrameCount() == 5)
        {
            BWAPI::Unitset takenMinerals;
            for (auto unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (unit->getType() == BWAPI::UnitTypes::Zerg_Drone)
                {
                    BWAPI::Unit best = nullptr;
                    int bestDist = INT_MAX;
                    for (auto minerals : BWAPI::Broodwar->getMinerals())
                    {
                        if (takenMinerals.find(minerals) != takenMinerals.end()) continue;

                        int dist = unit->getDistance(minerals);
                        if (dist < bestDist)
                        {
                            best = minerals;
                            bestDist = dist;
                        }
                    }
                    if (best)
                    {
                        takenMinerals.insert(best);
                        unit->gather(best);
                    }
                }
            }
        }

        BWAPI::Unit probe;
        for (auto unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (unit->getType() == BWAPI::UnitTypes::Protoss_Probe) probe = unit;
        }
        if (!probe) return;

        auto pylonTile = BWAPI::TilePosition(115, 20);
        auto pylonCenter = BWAPI::Position(pylonTile) + BWAPI::Position(32, 32);

        if (BWAPI::Broodwar->getFrameCount() == 500)
        {
            probe->move(pylonCenter);
        }

        if (BWAPI::Broodwar->getFrameCount() > 505)
        {
            std::cout << BWAPI::Broodwar->getFrameCount() << ": " << probe->getPosition() << std::endl;

            if (BWAPI::Broodwar->getFrameCount() == (564 - 9 - BWAPI::Broodwar->getLatencyFrames()))
            {
                probe->build(BWAPI::UnitTypes::Protoss_Pylon, pylonTile);
            }
            if (BWAPI::Broodwar->getFrameCount() == (564 - 9 - BWAPI::Broodwar->getLatencyFrames() + 1))
            {
                probe->move(BWAPI::Position(pylonTile) + BWAPI::Position(128, 0));
            }
        }
    };

    test.run();
}
