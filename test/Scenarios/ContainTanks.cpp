#include "BWTest.h"
#include "DoNothingModule.h"

#include "Map.h"
#include "Strategist.h"
#include "Plays/TestMainArmyAttackBasePlay.h"
#include "StrategyEngines/DoNothingStrategyEngine.h"

TEST(ContainTanks, FSHighGround)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Terran;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.map = Maps::GetOne("Fighting");
    test.randomSeed = 42;
    test.frameLimit = 800;
    test.expectWin = false;

    // Equal dragoon armies at each side of the choke
    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(103, 88)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(104, 88)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(105, 100)), // For vision on enemy army
    };

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode, BWAPI::Position(BWAPI::WalkPosition(420, 392)))
    };

    Base *baseToAttack = nullptr;

    // Order the dragoons to attack the bottom base
    test.onStartMine = [&baseToAttack]()
    {
        baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(118, 118)));

        Strategist::setStrategyEngine(std::make_unique<DoNothingStrategyEngine>());

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<TestMainArmyAttackBasePlay>(baseToAttack));
        Strategist::setOpening(openingPlays);
    };

    test.run();
}
