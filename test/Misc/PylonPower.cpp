#include "BWTest.h"

#include "DoNothingModule.h"

#include "Geo.h"
#include "UnitUtil.h"

namespace
{
    void processFrame(int &x, int &y, BWAPI::UnitType type)
    {
        BWAPI::TilePosition pylonTile(10, 116);

        // Create the pylon on frame 10
        if (BWAPI::Broodwar->getFrameCount() == 10)
        {
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                        BWAPI::UnitTypes::Protoss_Pylon,
                                        Geo::CenterOfUnit(pylonTile, BWAPI::UnitTypes::Protoss_Pylon));
            return;
        }

        if (y > 10) return;
        if (BWAPI::Broodwar->getFrameCount() < 20) return;
        if (BWAPI::Broodwar->getFrameCount() % 5 != 0) return;

        BWAPI::TilePosition buildingTile(pylonTile.x + x, pylonTile.y + y);

        // Try to create the next unit on the 10s
        if (BWAPI::Broodwar->getFrameCount() % 10 == 0)
        {
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                        type,
                                        Geo::CenterOfUnit(buildingTile, type));
            return;
        }

        // Check if the unit was created and assert
        bool found = false;
        for (auto unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (unit->getType() != type) continue;
            if (unit->getTilePosition() != buildingTile) continue;

            found = true;
            BWAPI::Broodwar->removeUnit(unit);
        }
        EXPECT_EQ(UnitUtil::Powers(pylonTile, buildingTile, type), found) << "Failed at offset (" << x << "," << y << ")";

        // Move to the next tile
        auto increment = [&]()
        {
            x++;
            if (x > 10)
            {
                x = -10;
                y++;
            }
        };
        increment();
        while (Geo::Overlaps(pylonTile, 2, 2, BWAPI::TilePosition(pylonTile.x + x, pylonTile.y + y), 4, 3))
        {
            increment();
        }
    }
}

TEST(PylonPower, Large)
{
    BWTest test;
    test.myModule = []()
    {
        return new DoNothingModule();
    };
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.opponentRace = BWAPI::Races::Zerg;
    test.map = Maps::GetOne("Fighting Spirit");
    test.randomSeed = 42;
    test.frameLimit = 5000;
    test.expectWin = false;

    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Observer, BWAPI::TilePosition(10, 116)), // For vision
    };

    test.removeStatic = {
            BWAPI::TilePosition(2, 114),
            BWAPI::TilePosition(1, 115),
            BWAPI::TilePosition(2, 116),
            BWAPI::TilePosition(1, 117),
            BWAPI::TilePosition(2, 118),
            BWAPI::TilePosition(1, 119),
            BWAPI::TilePosition(2, 120),
            BWAPI::TilePosition(2, 121),
            BWAPI::TilePosition(3, 122),
            BWAPI::TilePosition(7, 111),
    };

    int x = -10;
    int y = -10;
    test.onFrameMine = [&]()
    {
        processFrame(x, y, BWAPI::UnitTypes::Protoss_Gateway);
    };

    test.run();
}

TEST(PylonPower, Medium)
{
    BWTest test;
    test.myModule = []()
    {
        return new DoNothingModule();
    };
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.opponentRace = BWAPI::Races::Zerg;
    test.map = Maps::GetOne("Fighting Spirit");
    test.randomSeed = 42;
    test.frameLimit = 5000;
    test.expectWin = false;

    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Observer, BWAPI::TilePosition(10, 116)), // For vision
    };

    test.removeStatic = {
            BWAPI::TilePosition(2, 114),
            BWAPI::TilePosition(1, 115),
            BWAPI::TilePosition(2, 116),
            BWAPI::TilePosition(1, 117),
            BWAPI::TilePosition(2, 118),
            BWAPI::TilePosition(1, 119),
            BWAPI::TilePosition(2, 120),
            BWAPI::TilePosition(2, 121),
            BWAPI::TilePosition(3, 122),
            BWAPI::TilePosition(7, 111),
    };

    int x = -10;
    int y = -10;
    test.onFrameMine = [&]()
    {
        processFrame(x, y, BWAPI::UnitTypes::Protoss_Forge);
    };

    test.run();
}

TEST(PylonPower, Small)
{
    BWTest test;
    test.myModule = []()
    {
        return new DoNothingModule();
    };
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.opponentRace = BWAPI::Races::Zerg;
    test.map = Maps::GetOne("Fighting Spirit");
    test.randomSeed = 42;
    test.frameLimit = 5000;
    test.expectWin = false;

    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Observer, BWAPI::TilePosition(10, 116)), // For vision
    };

    test.removeStatic = {
            BWAPI::TilePosition(2, 114),
            BWAPI::TilePosition(1, 115),
            BWAPI::TilePosition(2, 116),
            BWAPI::TilePosition(1, 117),
            BWAPI::TilePosition(2, 118),
            BWAPI::TilePosition(1, 119),
            BWAPI::TilePosition(2, 120),
            BWAPI::TilePosition(2, 121),
            BWAPI::TilePosition(3, 122),
            BWAPI::TilePosition(7, 111),
    };

    int x = -10;
    int y = -10;
    test.onFrameMine = [&]()
    {
        processFrame(x, y, BWAPI::UnitTypes::Protoss_Photon_Cannon);
    };

    test.run();
}
