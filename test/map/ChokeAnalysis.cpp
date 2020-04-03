#include "BWTest.h"
#include "DoNothingModule.h"

#include <bwem.h>
#include "Choke.h"

void run(const std::string &map, BWAPI::TilePosition chokeLocation, std::function<void(const BWEM::ChokePoint*)> callback)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.myModule = []()
    {
        return new DoNothingModule();
    };
    test.map = map;
    test.frameLimit = 10;
    test.expectWin = false;
    test.writeReplay = false;

    test.onStartMine = [&]()
    {
        BWEM::Map::ResetInstance();
        BWEM::Map::Instance().Initialize(BWAPI::BroodwarPtr);
        BWEM::Map::Instance().EnableAutomaticPathAnalysis();
        BWEM::Map::Instance().FindBasesForStartingLocations();

        for (const auto &area : BWEM::Map::Instance().Areas())
        {
            for (const BWEM::ChokePoint *choke : area.ChokePoints())
            {
                if (chokeLocation.getApproxDistance(BWAPI::TilePosition(choke->Center())) < 5)
                {
                    callback(choke);
                    return;
                }
            }
        }

        EXPECT_FALSE(true);
    };

    test.run();
}

TEST(ChokeAnalysis, Python)
{
    run("maps/sscai/(4)Python.scx", BWAPI::TilePosition(66, 121), [](const BWEM::ChokePoint *bwemChoke)
    {
        auto choke = new Choke(bwemChoke);

        EXPECT_NE(choke->highElevationTile, BWAPI::TilePositions::Invalid);

        std::cout << "Choke found" << std::endl;
    });
    run("maps/sscai/(4)Python.scx", BWAPI::TilePosition(66, 121), [](const BWEM::ChokePoint *bwemChoke)
    {
        auto choke = new Choke(bwemChoke);

        EXPECT_NE(choke->highElevationTile, BWAPI::TilePositions::Invalid);

        std::cout << "Choke found" << std::endl;
    });
}

TEST(ChokeAnalysis, Benzene)
{
    run("maps/sscai/(2)Benzene.scx", BWAPI::TilePosition(96, 54), [](const BWEM::ChokePoint *bwemChoke)
    {
        auto choke = new Choke(bwemChoke);

        EXPECT_NE(choke->highElevationTile, BWAPI::TilePositions::Invalid);

        std::cout << "Choke found" << std::endl;
    });
    /*
    run("maps/sscai/(2)Benzene.scx", BWAPI::TilePosition(20, 78), [](const BWEM::ChokePoint *bwemChoke)
    {
        auto choke = new Choke(bwemChoke);

        EXPECT_NE(choke->highElevationTile, BWAPI::TilePositions::Invalid);

        std::cout << "Choke found" << std::endl;
    });
    run("maps/sscai/(2)Benzene.scx", BWAPI::TilePosition(107, 33), [](const BWEM::ChokePoint *bwemChoke)
    {
        auto choke = new Choke(bwemChoke);

        EXPECT_NE(choke->highElevationTile, BWAPI::TilePositions::Invalid);

        std::cout << "Choke found" << std::endl;
    });
     */
}

TEST(ChokeAnalysis, AnalyzeAll_Python)
{
    BWTest test;
    test.map = "maps/sscai/(4)Python.scx";
    test.frameLimit = 10;
    test.expectWin = false;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };

    test.run();
}

TEST(ChokeAnalysis, AnalyzeAll_Benzene)
{
    BWTest test;
    test.map = "maps/sscai/(2)Benzene.scx";
    test.frameLimit = 10;
    test.expectWin = false;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };

    test.run();
}

TEST(ChokeAnalysis, AnalyzeAll_TauCross)
{
    BWTest test;
    test.map = "maps/sscai/(3)Tau Cross.scx";
    test.frameLimit = 10;
    test.expectWin = false;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };

    test.run();
}

TEST(ChokeAnalysis, AnalyzeAll_LaMancha)
{
    BWTest test;
    test.map = "maps/sscai/(4)La Mancha1.1.scx";
    test.frameLimit = 10;
    test.expectWin = false;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };

    test.run();
}

TEST(ChokeAnalysis, AnalyzeAll_Destination)
{
    BWTest test;
    test.map = "maps/sscai/(2)Destination.scx";
    test.frameLimit = 10;
    test.expectWin = false;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };

    test.run();
}
