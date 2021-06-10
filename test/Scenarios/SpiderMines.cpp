#include "BWTest.h"
#include "DoNothingModule.h"
#include "DoNothingStrategyEngine.h"

#include "TestMainArmyAttackBasePlay.h"
#include "Map.h"
#include "Strategist.h"

TEST(SpiderMines, AttacksWithObserver)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Terran;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.map = Maps::GetOne("Fighting Spirit");
    test.randomSeed = 42;
    test.frameLimit = 300;

    // We have a few dragoons on their way into the enemy base and more on the way
    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(94, 86)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Observer, BWAPI::TilePosition(113, 95)),
    };

    // Enemy has a sunken wall
    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Vulture_Spider_Mine, BWAPI::TilePosition(113, 95)),
    };

    Base *baseToAttack = nullptr;

    // Order the dragoon to attack the bottom base
    test.onStartMine = [&baseToAttack]()
    {
        baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(117, 117)));

        Strategist::setStrategyEngine(std::make_unique<DoNothingStrategyEngine>());

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<TestMainArmyAttackBasePlay>(baseToAttack, true));
        Strategist::setOpening(openingPlays);
    };

    test.run();
}
