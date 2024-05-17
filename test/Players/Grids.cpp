#include "BWTest.h"
#include "DoNothingModule.h"

#include "Players.h"
#include "Map.h"
#include "Strategist.h"
#include "TestAttackBasePlay.h"

TEST(Grids, ObservedBurrowingLurker)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.map = Maps::GetOne("Fighting Spirit");
    test.randomSeed = 42;
    test.frameLimit = 100;
    test.expectWin = false;

    // We have a dragoon attacking
    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(105, 10))
    };

    // Enemy has a lurker
    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Lurker, BWAPI::TilePosition(102, 10))
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

    // Enemy orders the lurker to burrow immediately
    test.onFrameOpponent = []()
    {
        if (BWAPI::Broodwar->getFrameCount() != 5) return;

        for (auto &unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (unit->getType() == BWAPI::UnitTypes::Zerg_Lurker)
            {
                unit->burrow();
            }
        }
    };

    // Assert the threat grid
    test.onFrameMine = []()
    {
        auto &grid = Players::grid(BWAPI::Broodwar->enemy());
        if (BWAPI::Broodwar->getFrameCount() < 10)
        {
            EXPECT_EQ(grid.groundThreat(BWAPI::Position(BWAPI::TilePosition(102, 10))), 0);
            EXPECT_EQ( grid.staticGroundThreat(BWAPI::Position(BWAPI::TilePosition(102, 10))), 0);
        }
        else if (BWAPI::Broodwar->getFrameCount() > 50)
        {
            EXPECT_GT(grid.groundThreat(BWAPI::Position(BWAPI::TilePosition(102, 10))), 0);
            EXPECT_GT(grid.staticGroundThreat(BWAPI::Position(BWAPI::TilePosition(102, 10))), 0);
        }
    };

    test.run();
}

TEST(Grids, DiscoveredBurrowedLurker)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.map = Maps::GetOne("Fighting Spirit");
    test.randomSeed = 42;
    test.frameLimit = 500;
    test.expectWin = false;

    // We have a dragoon attacking
    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(123, 2))
    };

    // Enemy has a lurker
    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Lurker, BWAPI::TilePosition(102, 10))
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

    // Enemy orders the lurker to burrow immediately
    test.onFrameOpponent = []()
    {
        if (BWAPI::Broodwar->getFrameCount() != 5) return;

        for (auto &unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (unit->getType() == BWAPI::UnitTypes::Zerg_Lurker)
            {
                unit->burrow();
            }
        }
    };

    // Assert the threat grid
    test.onFrameMine = []()
    {
        auto &grid = Players::grid(BWAPI::Broodwar->enemy());
        if (BWAPI::Broodwar->getFrameCount() < 10)
        {
            EXPECT_EQ(grid.groundThreat(BWAPI::Position(BWAPI::TilePosition(102, 10))), 0);
            EXPECT_EQ( grid.staticGroundThreat(BWAPI::Position(BWAPI::TilePosition(102, 10))), 0);
        }
        else if (BWAPI::Broodwar->getFrameCount() > 150)
        {
            EXPECT_GT(grid.groundThreat(BWAPI::Position(BWAPI::TilePosition(102, 10))), 0);
            EXPECT_GT(grid.staticGroundThreat(BWAPI::Position(BWAPI::TilePosition(102, 10))), 0);
        }
    };

    test.run();
}
