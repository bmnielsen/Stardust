#include "BWTest.h"
#include "UAlbertaBotModule.h"

TEST(SunkenContain, Steamhammer)
{
    BWTest test;
    test.opponentName = "Steamhammer";
    test.map = Maps::GetOne("Mancha");
    test.randomSeed = 45965;
    test.frameLimit = 15000;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::StardustTestStrategyName = "4PoolHard";
        return module;
    };

    test.onFrameMine = []()
    {
        auto create = [](BWAPI::UnitType type, BWAPI::TilePosition pos)
        {
            UnitTypeAndPosition unit(type, BWAPI::Position(pos));
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->enemy(), unit.type, unit.getCenterPosition());
        };
        if (BWAPI::Broodwar->getFrameCount() == 6000)
        {
            create(BWAPI::UnitTypes::Zerg_Overlord, BWAPI::TilePosition(92, 8));
        }
        if (BWAPI::Broodwar->getFrameCount() == 6000)
        {
            create(BWAPI::UnitTypes::Zerg_Hatchery, BWAPI::TilePosition(92, 8));
        }
        if (BWAPI::Broodwar->getFrameCount() == 7000)
        {
            create(BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(96, 9));
        }
        if (BWAPI::Broodwar->getFrameCount() == 7010)
        {
            create(BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(95, 11));
        }
        if (BWAPI::Broodwar->getFrameCount() == 7020)
        {
            create(BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(98, 7));
        }
        if (BWAPI::Broodwar->getFrameCount() == 2930)
        {
            create(BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(99, 5));
        }
    };

    test.run();
}
