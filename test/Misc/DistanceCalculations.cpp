#include "BWTest.h"
#include "DoNothingModule.h"

#include "Geo.h"

TEST(DistanceCalculations, Zealots)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.map = Maps::GetOne("Tau Cross");
    test.randomSeed = 42;
    test.frameLimit = 400;
    test.expectWin = false;

    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(40, 34))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(40, 32))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(40, 30))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(40, 28))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(42, 34))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(42, 32))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(42, 30))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(42, 28))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(44, 34))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(44, 32))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(44, 30))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(44, 28))),
    };

    test.onStartMine = []()
    {
        for (auto &unit : BWAPI::Broodwar->self()->getUnits())
        {
            for (auto &other : BWAPI::Broodwar->self()->getUnits())
            {
                if (unit == other) continue;

                int bwapiEdgeToEdgeDistance = unit->getDistance(other);
                int bwapiEdgeToPointDistance = unit->getDistance(other->getPosition());
                int bwapiPointToPointDistance = unit->getPosition().getApproxDistance(other->getPosition());

                int geoEdgeToEdgeDistance = Geo::EdgeToEdgeDistance(unit->getType(), unit->getPosition(), other->getType(), other->getPosition());
                int geoEdgeToPointDistance = Geo::EdgeToPointDistance(unit->getType(), unit->getPosition(), other->getPosition());
                int geoPointToPointDistance = Geo::ApproximateDistance(unit->getPosition().x,
                                                                       other->getPosition().x,
                                                                       unit->getPosition().y,
                                                                       other->getPosition().y);

                EXPECT_EQ(bwapiEdgeToEdgeDistance, geoEdgeToEdgeDistance);
                EXPECT_EQ(bwapiEdgeToPointDistance, geoEdgeToPointDistance);
                EXPECT_EQ(bwapiPointToPointDistance, geoPointToPointDistance);
            }
        }
    };

    test.run();
}

TEST(DistanceCalculations, Mixed)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.map = Maps::GetOne("Tau Cross");
    test.randomSeed = 42;
    test.frameLimit = 400;
    test.expectWin = false;

    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(40, 34))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(40, 32))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::Position(BWAPI::TilePosition(40, 30))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Ultralisk, BWAPI::Position(BWAPI::TilePosition(40, 28))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Hydralisk, BWAPI::Position(BWAPI::TilePosition(42, 34))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Hero_Dark_Templar, BWAPI::Position(BWAPI::TilePosition(42, 32))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::Position(BWAPI::TilePosition(42, 30))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Medic, BWAPI::Position(BWAPI::TilePosition(42, 28))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode, BWAPI::Position(BWAPI::TilePosition(44, 34))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Archon, BWAPI::Position(BWAPI::TilePosition(44, 32))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Reaver, BWAPI::Position(BWAPI::TilePosition(44, 30))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Vulture, BWAPI::Position(BWAPI::TilePosition(44, 28))),
    };

    test.onStartMine = []()
    {
        for (auto &unit : BWAPI::Broodwar->self()->getUnits())
        {
            for (auto &other : BWAPI::Broodwar->self()->getUnits())
            {
                if (unit == other) continue;

                int bwapiEdgeToEdgeDistance = unit->getDistance(other);
                int bwapiEdgeToPointDistance = unit->getDistance(other->getPosition());
                int bwapiPointToPointDistance = unit->getPosition().getApproxDistance(other->getPosition());

                int geoEdgeToEdgeDistance = Geo::EdgeToEdgeDistance(unit->getType(), unit->getPosition(), other->getType(), other->getPosition());
                int geoEdgeToPointDistance = Geo::EdgeToPointDistance(unit->getType(), unit->getPosition(), other->getPosition());
                int geoPointToPointDistance = Geo::ApproximateDistance(unit->getPosition().x,
                                                                       other->getPosition().x,
                                                                       unit->getPosition().y,
                                                                       other->getPosition().y);

                EXPECT_EQ(bwapiEdgeToEdgeDistance, geoEdgeToEdgeDistance);
                EXPECT_EQ(bwapiEdgeToPointDistance, geoEdgeToPointDistance);
                EXPECT_EQ(bwapiPointToPointDistance, geoPointToPointDistance);
            }
        }
    };

    test.run();
}

TEST(DistanceCalculations, Positions)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.map = Maps::GetOne("Tau Cross");
    test.randomSeed = 42;
    test.frameLimit = 400;
    test.expectWin = false;

    test.onStartMine = []()
    {
        std::vector<BWAPI::Position> positions;
        for (int x = -40; x <= 40; x++)
        {
            for (int y = -40; y <= 40; y++)
            {
                positions.emplace_back(x, y);
            }
        }

        for (auto first : positions)
        {
            for (auto second : positions)
            {
                int bwapiDistance = first.getApproxDistance(second);
                int geoDistance = Geo::ApproximateDistance(first.x, second.x, first.y, second.y);
                EXPECT_EQ(bwapiDistance, geoDistance) << first << " and " << second;
            }
        }
    };

    test.run();
}
