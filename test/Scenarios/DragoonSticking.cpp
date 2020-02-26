#include "BWTest.h"
#include "DoNothingModule.h"

#include "Geo.h"
#include "Map.h"
#include "Strategist.h"
#include "TestAttackBasePlay.h"

TEST(DragoonSticking, StuckOnGateway)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.map = "maps/sscai/(4)Fighting Spirit.scx";
    test.frameLimit = 400;
    test.expectWin = false;

    // Simulate a dragoon that just popped out of the top gateway
    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::TilePosition(109, 6)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Gateway, BWAPI::TilePosition(111, 4)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Gateway, BWAPI::TilePosition(111, 7)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(115, 7))
    };

    Base *baseToAttack = nullptr;

    // Order the dragoon to attack the bottom base
    test.onStartMine = [&baseToAttack]()
    {
        baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(117, 117)));

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<TestAttackBasePlay>(baseToAttack));
        Strategist::setOpening(openingPlays);
    };

    // Verify the dragoon got past the gateway
    test.onEndMine = [](bool won)
    {
        for (auto unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (unit->getType() != BWAPI::UnitTypes::Protoss_Dragoon) continue;

            auto dist = unit->getDistance(BWAPI::Position(BWAPI::TilePosition(117, 117)));
            EXPECT_LT(dist, 2500);
        }
    };

    test.run();
}
