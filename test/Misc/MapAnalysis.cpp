#include <utility>

#include "BWTest.h"
#include "DoNothingModule.h"

std::pair<BWAPI::TilePosition, int> getStartPositionAndSeed(std::shared_ptr<Maps::MapMetadata> map)
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
    test.map = std::move(map);
    test.frameLimit = 10;
    test.expectWin = false;
    test.writeReplay = false;

    BWAPI::TilePosition tile;
    test.onStartMine = [&tile]()
    {
        tile = BWAPI::Broodwar->self()->getStartLocation();
    };

    test.run();

    return std::make_pair(tile, test.randomSeed);
}

TEST(MapAnalysis, FindStartLocationSeeds)
{
    auto maps = Maps::Get("aiide");
    for (auto &map : maps)
    {
        std::map<BWAPI::TilePosition, int> startPositionToSeed;
        while (startPositionToSeed.size() < map.startLocationSeeds.size())
        {
            auto tileAndSeed = getStartPositionAndSeed(std::make_shared<Maps::MapMetadata>(map));
            if (startPositionToSeed.find(tileAndSeed.first) == startPositionToSeed.end())
            {
                startPositionToSeed[tileAndSeed.first] = tileAndSeed.second;
            }
        }

        std::vector<int> startPositionSeeds(startPositionToSeed.size());
        for (auto &pair : startPositionToSeed)
        {
            startPositionSeeds.push_back(pair.second);
        }
        map.startLocationSeeds = startPositionSeeds;
    }

    for (auto &map : maps)
    {
        std::ostringstream ss;
        ss << map.shortfilename() << ": {";
        bool first = true;
        for (auto &seed : map.startLocationSeeds)
        {
            if (!first) ss << ",";
            first = false;
            ss << seed;
        }
        ss << "}";
        std::cout << ss.str() << std::endl;
    }
}

TEST(MapAnalysis, GetMapHashes)
{
    std::vector<std::pair<std::string, std::string>> mapsAndHashes;
    Maps::RunOnEach(Maps::Get("aiide"), [&](BWTest test)
    {
        test.myModule = []()
        {
            return new DoNothingModule();
        };
        test.opponentModule = []()
        {
            return new DoNothingModule();
        };
        test.frameLimit = 10;
        test.expectWin = false;
        test.writeReplay = false;

        test.onStartMine = [&]()
        {
            mapsAndHashes.emplace_back(std::make_pair(test.map->shortfilename(), BWAPI::Broodwar->mapHash()));
        };

        test.run();
    });

    for (auto &pair : mapsAndHashes)
    {
        std::cout << pair.first << ": " << pair.second << std::endl;
    }
}
