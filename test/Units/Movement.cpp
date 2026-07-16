#include "BWTest.h"
#include "DoNothingModule.h"
#include "Units.h"
#include "Workers.h"

TEST(Unit_Movement, DestinationBlockedChoke)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.map = Maps::GetOne("Destination");
    test.randomSeed = 4349;
    test.frameLimit = 1000;
    test.expectWin = false;

    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(BWAPI::WalkPosition(28, 489))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Overlord, BWAPI::Position(256, 2864), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Templar_Archives, BWAPI::Position(368, 2976), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::Position(128, 3136), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Photon_Cannon, BWAPI::Position(160, 2944), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Photon_Cannon, BWAPI::Position(160, 2784), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::Position(352, 2880), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Nexus, BWAPI::Position(256, 2864), true),
    };

    test.onStartMine = []()
    {
        for (auto unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (unit->getType().isWorker()) BWAPI::Broodwar->killUnit(unit);
        }
    };

    test.onFrameMine = []()
    {
        if (BWAPI::Broodwar->getFrameCount() > 10)
        {
            for (auto &unit : Units::allMineCompletedOfType(BWAPI::UnitTypes::Protoss_Probe))
            {
                auto worker = std::dynamic_pointer_cast<MyWorkerImpl>(unit);
                if (worker)
                {
                    Workers::reserveWorker(worker);
                    CherryVis::log(worker->id) << "moveTo (348,390)";
                    worker->moveTo(BWAPI::Position(BWAPI::WalkPosition(348, 390)));
                }
            }
        }
    };

    test.run();
}
