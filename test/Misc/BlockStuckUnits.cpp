#include "BWTest.h"

#include "DoNothingModule.h"

#include "Map.h"
#include "Units.h"
#include "Strategist.h"
#include "TestAttackBasePlay.h"

// Tests that zealots produced in the top row of a full large block can get out
TEST(BlockStuckUnits, ZealotInLargeBlock)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.opponentRace = BWAPI::Races::Zerg;
    test.map = "maps/sscai/(4)Fighting Spirit.scx";
    test.frameLimit = 3000;
    test.expectWin = false;

    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(102, 0)), // For vision
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::TilePosition(106, 4)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::TilePosition(108, 4)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Gateway, BWAPI::TilePosition(100, 1)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Gateway, BWAPI::TilePosition(104, 1)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Gateway, BWAPI::TilePosition(108, 1)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Gateway, BWAPI::TilePosition(112, 1)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Gateway, BWAPI::TilePosition(100, 6)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Gateway, BWAPI::TilePosition(104, 6)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Gateway, BWAPI::TilePosition(108, 6)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Gateway, BWAPI::TilePosition(112, 6)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Arbiter_Tribunal, BWAPI::TilePosition(100, 4)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Arbiter_Tribunal, BWAPI::TilePosition(103, 4)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Arbiter_Tribunal, BWAPI::TilePosition(110, 4)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Arbiter_Tribunal, BWAPI::TilePosition(113, 4)),
    };

    Base *baseToAttack = nullptr;

    // Order them to attack the bottom base
    test.onStartMine = [&baseToAttack]()
    {
        baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(117, 117)));

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<TestAttackBasePlay>(baseToAttack));
        Strategist::setOpening(openingPlays);
    };

    bool createdZealots = false;

    test.onFrameMine = [&createdZealots]()
    {
        // Create the zealots when we can afford them
        if (!createdZealots && BWAPI::Broodwar->self()->minerals() >= 400)
        {
            // Build from all gateways in the top row
            for (auto &unit : Units::allMine())
            {
                if (unit->type == BWAPI::UnitTypes::Protoss_Gateway && unit->buildTile.y == 1)
                {
                    unit->train(BWAPI::UnitTypes::Protoss_Zealot);
                }
            }

            createdZealots = true;
        }
    };

    // Verify the zealots got out of our base
    test.onEndMine = [](bool won)
    {
        int zealotCount = 0;
        for (auto unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (unit->getType() != BWAPI::UnitTypes::Protoss_Zealot) continue;

            zealotCount++;

            auto dist = unit->getDistance(BWAPI::Position(BWAPI::TilePosition(117, 117)));
            EXPECT_LT(dist, 2500);
        }

        EXPECT_EQ(zealotCount, 4);
    };

    test.run();
}
