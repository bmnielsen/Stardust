#include "BWTest.h"
#include "DoNothingModule.h"

#include "BuildingPlacement.h"

TEST(ForgeGatewayWalls, AllSSCAIT)
{
    Maps::RunOnEachStartLocation(Maps::Get("sscai"), [](BWTest test)
    {
        test.opponentModule = []()
        {
            return new DoNothingModule();
        };
        test.frameLimit = 10;
        test.expectWin = false;

        test.onStartMine = [&test]()
        {
            test.addClockPositionToReplayName();
        };

        test.onFrameMine = []() {
            if (currentFrame == 0)
            {
                auto wall = BuildingPlacement::createForgeGatewayWall(true);
                std::cout << "Generated wall: " << wall << std::endl;
            }
        };
        test.run();
    });
}

TEST(ForgeGatewayWalls, Benzene8Oclock)
{
    BWTest test;
    test.randomSeed = 1617;
    test.map = Maps::GetOne("Benzene");
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.frameLimit = 10;
    test.expectWin = false;
    test.onFrameMine = []() {
        if (currentFrame == 0)
        {
            auto wall = BuildingPlacement::createForgeGatewayWall(true);
            std::cout << "Generated wall: " << wall << std::endl;
        }
    };
    test.run();
}
