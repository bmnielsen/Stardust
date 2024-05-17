#include "BWTest.h"

#include "DoNothingModule.h"
#include "PathFinding.h"

TEST(DistanceCalculations, BlueStorm)
{
    BWTest test;
    test.map = Maps::GetOne("BlueStorm");
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.frameLimit = 10;
    test.expectWin = false;

    test.onStartMine = []()
    {
        int dist = PathFinding::GetGroundDistance(BWAPI::Position(BWAPI::WalkPosition(115, 271)),
                                                  BWAPI::Position(BWAPI::WalkPosition(375, 102)));

        EXPECT_GT(dist, 3000);
    };
    test.run();
}
