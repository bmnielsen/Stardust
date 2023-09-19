#include "BWTest.h"
#include "DoNothingModule.h"

#include "PathFinding.h"
#include "Players.h"
#include "Map.h"

TEST(PathFindingSearch, FindPath_NonOptimalWhenDiagonal)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.map = Maps::GetOne("Fighting Spirit");
    test.randomSeed = 42;
    test.frameLimit = 10;
    test.expectWin = false;
    test.writeReplay = false;

    test.onStartMine = []()
    {
        auto isWalkableTile = [](BWAPI::TilePosition tile)
        {
            return Map::isWalkable(tile);
        };
        auto path = PathFinding::Search(BWAPI::TilePosition(BWAPI::WalkPosition(453, 384)),
                                        BWAPI::TilePosition(BWAPI::WalkPosition(420, 408)),
                                        isWalkableTile);
        EXPECT_FALSE(path.empty());
    };

    test.run();
}

TEST(PathFindingSearch, FindPath_ExplodingComplexity)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.map = Maps::GetOne("Fighting Spirit");
    test.randomSeed = 42;
    test.frameLimit = 10;
    test.expectWin = false;
    test.writeReplay = false;

    test.onStartMine = []()
    {
        auto isWalkableTile = [](BWAPI::TilePosition tile)
        {
            return Map::isWalkable(tile);
        };
        auto path = PathFinding::Search(BWAPI::TilePosition(118,122),
                                        BWAPI::TilePosition(117,88),
                                        isWalkableTile);
        EXPECT_FALSE(path.empty());
    };

    test.run();
}

TEST(PathFindingSearch, FindPath_Profiling)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.map = Maps::GetOne("Destination");
    test.randomSeed = 42;
    test.frameLimit = 10;
    test.expectWin = false;
    test.writeReplay = false;

    test.onStartMine = []()
    {
        auto &grid = Players::grid(BWAPI::Broodwar->enemy());
        auto avoidThreatTiles = [&grid](BWAPI::TilePosition tile)
        {
            return grid.airThreat((tile.x << 2U) + 2, (tile.y << 2U) + 2) == 0 &&
                   grid.detection((tile.x << 2U) + 2, (tile.y << 2U) + 2) == 0;
        };
        auto path = PathFinding::Search(BWAPI::TilePosition(59,118),
                                        BWAPI::TilePosition(88,5),
                                        avoidThreatTiles);
        EXPECT_FALSE(path.empty());
    };

    test.run();
}
