#include "BWTest.h"
#include "DoNothingModule.h"

#include "PathFinding.h"

TEST(PathFindingSearch, FindPath_NonOptimalWhenDiagonal)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.frameLimit = 10;
    test.expectWin = false;
    test.writeReplay = false;

    test.onStartMine = []()
    {
        auto path = PathFinding::Search(BWAPI::TilePosition(BWAPI::WalkPosition(453, 384)),
                                        BWAPI::TilePosition(BWAPI::WalkPosition(420, 408)));
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
    test.frameLimit = 10;
    test.expectWin = false;
    test.writeReplay = false;

    test.onStartMine = []()
    {
        auto path = PathFinding::Search(BWAPI::TilePosition(118,122),
                                        BWAPI::TilePosition(118,88));
        EXPECT_FALSE(path.empty());
    };

    test.run();
}
