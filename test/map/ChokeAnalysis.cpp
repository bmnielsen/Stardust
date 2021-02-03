#include "BWTest.h"
#include "DoNothingModule.h"

#include <bwem.h>
#include "Choke.h"
#include "Map.h"

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
    test.map = Maps::GetOne(map);
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
    run("Python", BWAPI::TilePosition(66, 121), [](const BWEM::ChokePoint *bwemChoke)
    {
        auto choke = new Choke(bwemChoke);

        EXPECT_NE(choke->highElevationTile, BWAPI::TilePositions::Invalid);

        std::cout << "Choke found" << std::endl;
    });
    run("Python", BWAPI::TilePosition(66, 121), [](const BWEM::ChokePoint *bwemChoke)
    {
        auto choke = new Choke(bwemChoke);

        EXPECT_NE(choke->highElevationTile, BWAPI::TilePositions::Invalid);

        std::cout << "Choke found" << std::endl;
    });
}

TEST(ChokeAnalysis, Benzene)
{
    run("Benzene", BWAPI::TilePosition(96, 54), [](const BWEM::ChokePoint *bwemChoke)
    {
        auto choke = new Choke(bwemChoke);

        EXPECT_NE(choke->highElevationTile, BWAPI::TilePositions::Invalid);

        std::cout << "Choke found" << std::endl;
    });
    /*
    run("maps/sscai/(2)Benzene", BWAPI::TilePosition(20, 78), [](const BWEM::ChokePoint *bwemChoke)
    {
        auto choke = new Choke(bwemChoke);

        EXPECT_NE(choke->highElevationTile, BWAPI::TilePositions::Invalid);

        std::cout << "Choke found" << std::endl;
    });
    run("maps/sscai/(2)Benzene", BWAPI::TilePosition(107, 33), [](const BWEM::ChokePoint *bwemChoke)
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
    test.map = Maps::GetOne("Python");
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
    test.map = Maps::GetOne("Benzene");
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
    test.map = Maps::GetOne("Tau Cross");
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
    test.map = Maps::GetOne("La Mancha");
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
    test.map = Maps::GetOne("Destination");
    test.frameLimit = 10;
    test.expectWin = false;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };

    test.run();
}

TEST(ChokeAnalysis, AnalyzeAll_NeoMoonGlaive)
{
    BWTest test;
    test.map = Maps::GetOne("Moon Glaive");
    test.frameLimit = 10;
    test.expectWin = false;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };

    test.onEndMine = [](bool win)
    {
        EXPECT_FALSE(Map::isInNarrowChoke(BWAPI::TilePosition(80, 55)));
    };

    test.run();
}

TEST(ChokeAnalysis, AnalyzeAll_BlueStorm)
{
    BWTest test;
    test.map = Maps::GetOne("BlueStorm");
    test.frameLimit = 10;
    test.expectWin = false;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };

    test.onEndMine = [](bool win)
    {
        EXPECT_FALSE(Map::isInNarrowChoke(BWAPI::TilePosition(80, 55)));
    };

    test.run();
}

TEST(ChokeAnalysis, MatchPoint)
{
    run("Match", BWAPI::TilePosition(2, 91), [](const BWEM::ChokePoint *bwemChoke)
    {
        auto choke = new Choke(bwemChoke);
        choke->setAsMainChoke();

        std::cout << "Choke found" << std::endl;
    });
}

TEST(ChokeAnalysis, AnalyzeAll_MatchPoint)
{
    BWTest test;
    test.map = Maps::GetOne("Match");
    test.frameLimit = 10;
    test.randomSeed = 6903;
    test.expectWin = false;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };

    test.onEndMine = [](bool win)
    {
        EXPECT_FALSE(Map::isInNarrowChoke(BWAPI::TilePosition(80, 55)));
    };

    test.run();
}

TEST(ChokeAnalysis, AnalyzeAll)
{
    Maps::RunOnEachStartLocation(Maps::Get("aists4"), [&](BWTest test)
    {
        test.frameLimit = 10;
        test.expectWin = false;
        test.opponentModule = []()
        {
            return new DoNothingModule();
        };

        test.onEndMine = [](bool win)
        {
            EXPECT_FALSE(Map::isInNarrowChoke(BWAPI::TilePosition(BWAPI::Broodwar->mapWidth() / 2, BWAPI::Broodwar->mapHeight() / 2)));
        };

        test.run();
    });
}

TEST(ChokeAnalysis, Medusa)
{
    run("Medusa", BWAPI::TilePosition(BWAPI::WalkPosition(202,162)), [](const BWEM::ChokePoint *bwemChoke)
    {
        new Choke(bwemChoke);

        std::cout << "Choke found" << std::endl;
    });
}
