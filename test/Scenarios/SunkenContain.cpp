#include "BWTest.h"
#include "UAlbertaBotModule.h"

TEST(SunkenContain, Steamhammer)
{
    BWTest test;
    test.opponentName = "Steamhammer";
    test.map = Maps::GetOne("Spirit");
    test.randomSeed = 50443;
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
        if (BWAPI::Broodwar->getFrameCount() == 2000)
        {
            create(BWAPI::UnitTypes::Zerg_Overlord, BWAPI::TilePosition(110, 88));
        }
        if (BWAPI::Broodwar->getFrameCount() == 2010)
        {
            create(BWAPI::UnitTypes::Zerg_Hatchery, BWAPI::TilePosition(110, 88));
        }
        if (BWAPI::Broodwar->getFrameCount() == 2900)
        {
            create(BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(107, 91));
        }
        if (BWAPI::Broodwar->getFrameCount() == 2910)
        {
            create(BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(109, 91));
        }
        if (BWAPI::Broodwar->getFrameCount() == 2920)
        {
            create(BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(111, 91));
        }
        if (BWAPI::Broodwar->getFrameCount() == 2930)
        {
            create(BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(113, 91));
        }
        if (BWAPI::Broodwar->getFrameCount() == 2940)
        {
            create(BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(107, 93));
        }
        if (BWAPI::Broodwar->getFrameCount() == 2950)
        {
            create(BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(109, 93));
        }
        if (BWAPI::Broodwar->getFrameCount() == 2960)
        {
            create(BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(111, 93));
        }
        if (BWAPI::Broodwar->getFrameCount() == 2970)
        {
            create(BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(113, 93));
        }
        if (BWAPI::Broodwar->getFrameCount() == 2980)
        {
            create(BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(106, 89));
        }
        if (BWAPI::Broodwar->getFrameCount() == 2990)
        {
            create(BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(112, 89));
        }
    };

    test.run();
}
