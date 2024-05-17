#include "BWTest.h"

#include "DoNothingModule.h"

#include "Units.h"

TEST(Cannons, TestTimings)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.opponentRace = BWAPI::Races::Terran;
    test.map = Maps::GetOne("Heartbreak");
    test.randomSeed = 42;
    test.frameLimit = 1000;
    test.expectWin = false;

    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::TilePosition(115, 55)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Photon_Cannon, BWAPI::TilePosition(113, 55)),
    };

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::TilePosition(113, 43)),
    };

    test.onFrameMine = []()
    {
        MyUnit cannon = nullptr;
        for (auto &unit : Units::allMine())
        {
            if (unit->type != BWAPI::UnitTypes::Protoss_Photon_Cannon) continue;
            cannon = unit;
            break;
        }
        if (!cannon) return;

        for (auto &unit : Units::allEnemy())
        {
            CherryVis::log(cannon->id) << "Dist to " << unit->type << ": " << cannon->getDistance(unit);
        }
    };

    test.onFrameOpponent = []()
    {
        for (auto &unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (unit->getType() != BWAPI::UnitTypes::Terran_SCV) continue;
            if (unit->getDistance(BWAPI::Position(BWAPI::TilePosition(113, 55))) > 1000) continue;
            unit->move(BWAPI::Position(BWAPI::TilePosition(113, 55)));
            return;
        }
    };

    test.run();
}
