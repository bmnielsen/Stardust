#include "BWTest.h"
#include "DoNothingModule.h"

#include "Map.h"
#include "Strategist.h"
#include "PathFinding.h"
#include "Plays/Offensive/AttackMainBase.h"

namespace
{
    void assertUnitDistances(BWAPI::UnitType type, int threshold)
    {
        for (auto unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (unit->getType() != type) continue;

            for (auto other : BWAPI::Broodwar->self()->getUnits())
            {
                if (other->getType() != type) continue;

                EXPECT_LT(unit->getDistance(other), threshold) << unit->getType() << "@" << unit->getTilePosition() << " vs. "
                                                               << other->getType() << "@" << other->getTilePosition();
            }
        }
    }

    void computeDistances(BWAPI::UnitType type, BWAPI::Position goal, int *avg, int *min, int *max)
    {
        *avg = 0;
        *min = INT_MAX;
        *max = 0;
        int count = 0;

        for (auto unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (unit->getType() != type) continue;

            int dist = PathFinding::GetGroundDistance(unit->getPosition(), goal, unit->getType());
            if (dist == -1) continue;

            if (dist < *min) *min = dist;
            if (dist > *max) *max = dist;
            *avg += dist;
            count++;
        }

        *avg /= count;
    }
}

TEST(SquadMovement, UnitsStayTogether_Dragoons)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.map = "maps/sscai/(3)Tau Cross.scx";
    test.frameLimit = 400;
    test.expectWin = false;

    // Start the units in a conga line
    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(42, 34))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(42, 32))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(42, 30))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(42, 28))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(42, 26))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(42, 24)))
    };

    Base *baseToAttack = nullptr;

    // Order them to attack the bottom base
    test.onStartMine = [&baseToAttack]()
    {
        baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(93, 118)));

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<AttackMainBase>(baseToAttack));
        Strategist::setOpening(openingPlays);
    };

    // Verify the units are now moving as a cohesive unit
    test.onEndMine = [&baseToAttack](bool won)
    {
        assertUnitDistances(BWAPI::UnitTypes::Protoss_Dragoon, 128);

        // Without flocking the distances are avg=2140; min=1940; max=2318
        // With flocking we accept the units to not be quite as close, but they should still move quickly
        int avg, min, max;
        computeDistances(BWAPI::UnitTypes::Protoss_Dragoon, baseToAttack->getPosition(), &avg, &min, &max);
        std::cout << "Dragoon distances: avg=" << avg << "; min=" << min << "; max=" << max << std::endl;
        EXPECT_LT(min, 2300);
        EXPECT_LT(max, 2500);
    };

    test.run();
}

TEST(SquadMovement, UnitsStayTogether_Zealots)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.map = "maps/sscai/(3)Tau Cross.scx";
    test.frameLimit = 400;
    test.expectWin = false;

    // Start the units in a conga line
    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(42, 34))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(42, 32))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(42, 30))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(42, 28))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(42, 26))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(42, 24)))
    };

    Base *baseToAttack = nullptr;

    // Order them to attack the bottom base
    test.onStartMine = [&baseToAttack]()
    {
        baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(93, 118)));

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<AttackMainBase>(baseToAttack));
        Strategist::setOpening(openingPlays);
    };

    // Verify the units are now moving as a cohesive unit
    test.onEndMine = [&baseToAttack](bool won)
    {
        assertUnitDistances(BWAPI::UnitTypes::Protoss_Zealot, 96);

        // Without flocking the distances are avg=2522; min=2366; max=2666
        // With flocking we accept the units to not be quite as close, but they should still move quickly
        int avg, min, max;
        computeDistances(BWAPI::UnitTypes::Protoss_Zealot, baseToAttack->getPosition(), &avg, &min, &max);
        std::cout << "Zealot distances: avg=" << avg << "; min=" << min << "; max=" << max << std::endl;
        EXPECT_LT(min, 2650);
        EXPECT_LT(max, 2750);
    };

    test.run();
}

TEST(SquadMovement, UnitsStayTogether_Mixed)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.map = "maps/sscai/(3)Tau Cross.scx";
    test.frameLimit = 400;
    test.expectWin = false;

    // Start the units in a conga line
    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(42, 34))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(42, 32))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(42, 30))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(42, 28))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(42, 26))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(42, 24))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(43, 34))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(43, 32))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(43, 30))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(43, 28))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(43, 26))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(43, 24)))
    };

    Base *baseToAttack = nullptr;

    // Order them to attack the bottom base
    test.onStartMine = [&baseToAttack]()
    {
        baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(93, 118)));

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<AttackMainBase>(baseToAttack));
        Strategist::setOpening(openingPlays);
    };

    // Verify the units are now moving as a cohesive unit
    test.onEndMine = [&baseToAttack](bool won)
    {
        assertUnitDistances(BWAPI::UnitTypes::Protoss_Dragoon, 200);
        assertUnitDistances(BWAPI::UnitTypes::Protoss_Zealot, 200);

        int avg, min, max;

        // Zealot distances: avg=2545; min=2507; max=2597
        // Dragoon distances: avg=2443; min=2306; max=2550

        computeDistances(BWAPI::UnitTypes::Protoss_Zealot, baseToAttack->getPosition(), &avg, &min, &max);
        std::cout << "Zealot distances: avg=" << avg << "; min=" << min << "; max=" << max << std::endl;
        EXPECT_LT(min, 2700);
        EXPECT_LT(max, 2800);

        computeDistances(BWAPI::UnitTypes::Protoss_Dragoon, baseToAttack->getPosition(), &avg, &min, &max);
        std::cout << "Dragoon distances: avg=" << avg << "; min=" << min << "; max=" << max << std::endl;
        EXPECT_LT(min, 2700);
        EXPECT_LT(max, 2800);
    };

    test.run();
}

TEST(SquadMovement, OrphanedUnit)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.frameLimit = 400;
    test.expectWin = false;

    // Start the units on opposite sides of a wall
    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(94, 19))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(94, 20))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(101, 18)))
    };

    Base *baseToAttack = nullptr;

    // Order them to attack the bottom-right base
    test.onStartMine = [&baseToAttack]()
    {
        baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(117, 117)));

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<AttackMainBase>(baseToAttack));
        Strategist::setOpening(openingPlays);
    };

    // Verify the units are all closer
    test.onEndMine = [&baseToAttack](bool won)
    {
        int avg, min, max;
        computeDistances(BWAPI::UnitTypes::Protoss_Zealot, baseToAttack->getPosition(), &avg, &min, &max);

        // Assert the orphaned unit made it out
        EXPECT_LT(max, 3500);
    };

    test.run();
}

// TODO: Improve flocking behaviour of large groups and add some assertions to this test
TEST(SquadMovement, DragoonBall)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.frameLimit = 600;
    test.expectWin = false;

    // Start the dragoons in a tight ball
    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(88, 19)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(88, 20)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(88, 21)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(88, 22)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(88, 23)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(89, 19)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(89, 20)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(89, 21)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(89, 22)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(89, 23)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(90, 19)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(90, 20)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(90, 21)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(90, 22)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(90, 23)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(91, 19)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(91, 20)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(91, 21)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(91, 22)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(91, 23)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(92, 19)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(92, 20)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(92, 21)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(92, 22)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(92, 23)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(93, 19)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(93, 20)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(93, 21)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(93, 22)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(93, 23)),
    };

    Base *baseToAttack = nullptr;

    // Order them to attack the bottom-right base
    test.onStartMine = [&baseToAttack]()
    {
        baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(117, 117)));

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<AttackMainBase>(baseToAttack));
        Strategist::setOpening(openingPlays);
    };

    // Verify the units have moved efficiently
    test.onEndMine = [&baseToAttack](bool won)
    {
        // Without flocking the distances are avg=1913; min=1411; max=2386
        // default: 2018;1773;2187

        int avg, min, max;
        computeDistances(BWAPI::UnitTypes::Protoss_Dragoon, baseToAttack->getPosition(), &avg, &min, &max);
        std::cout << "avg=" << avg << "; min=" << min << "; max=" << max << std::endl;

        // Assert the orphaned unit made it out
        //EXPECT_LT(max, 3500);
    };

    test.run();
}
