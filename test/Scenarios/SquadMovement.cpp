#include "BWTest.h"
#include "DoNothingModule.h"

#include "Map.h"
#include "Strategist.h"
#include "PathFinding.h"
#include "Plays/Offensive/AttackBase.h"

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
            std::make_pair(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(42, 34))),
            std::make_pair(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(42, 32))),
            std::make_pair(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(42, 30))),
            std::make_pair(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(42, 28))),
            std::make_pair(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(42, 26))),
            std::make_pair(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(42, 24)))
    };

    Base *baseToAttack = nullptr;

    // Order them to attack the bottom base
    test.onStartMine = [&baseToAttack]()
    {
        baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(93, 118)));

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<AttackBase>(baseToAttack));
        Strategist::setOpening(openingPlays);
    };

    // Verify the units are now moving as a cohesive unit
    test.onEndMine = [&baseToAttack]()
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
            std::make_pair(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(42, 34))),
            std::make_pair(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(42, 32))),
            std::make_pair(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(42, 30))),
            std::make_pair(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(42, 28))),
            std::make_pair(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(42, 26))),
            std::make_pair(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(42, 24)))
    };

    Base *baseToAttack = nullptr;

    // Order them to attack the bottom base
    test.onStartMine = [&baseToAttack]()
    {
        baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(93, 118)));

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<AttackBase>(baseToAttack));
        Strategist::setOpening(openingPlays);
    };

    // Verify the units are now moving as a cohesive unit
    test.onEndMine = [&baseToAttack]()
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
            std::make_pair(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(42, 34))),
            std::make_pair(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(42, 32))),
            std::make_pair(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(42, 30))),
            std::make_pair(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(42, 28))),
            std::make_pair(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(42, 26))),
            std::make_pair(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(42, 24))),
            std::make_pair(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(43, 34))),
            std::make_pair(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(43, 32))),
            std::make_pair(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(43, 30))),
            std::make_pair(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(43, 28))),
            std::make_pair(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(43, 26))),
            std::make_pair(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(43, 24)))
    };

    Base *baseToAttack = nullptr;

    // Order them to attack the bottom base
    test.onStartMine = [&baseToAttack]()
    {
        baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(93, 118)));

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<AttackBase>(baseToAttack));
        Strategist::setOpening(openingPlays);
    };

    // Verify the units are now moving as a cohesive unit
    test.onEndMine = [&baseToAttack]()
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
