#include <utility>

#include "BWTest.h"
#include "DoNothingModule.h"

namespace
{
    void out(const std::vector<Maps::MapMetadata> &maps)
    {
        std::ostringstream os;

        auto thirdSeedLayer = [&os](const std::array<int, 3> &seeds)
        {
            os << "{" << seeds[0] << "," << seeds[1] << "," << seeds[2] << "}";
        };
        auto secondSeedLayer = [&os, &thirdSeedLayer](const std::vector<std::array<int, 3>> &seeds)
        {
            os << "{";
            std::string sep;
            for (const auto &thirdLayer : seeds)
            {
                os << sep;
                thirdSeedLayer(thirdLayer);
                sep = ",";
            }
            os << "}";
        };
        auto firstSeedLayer = [&os, &secondSeedLayer](const std::vector<std::vector<std::array<int, 3>>> &seeds)
        {
            os << "{";
            std::string sep;
            for (const auto &secondLayer : seeds)
            {
                os << sep;
                secondSeedLayer(secondLayer);
                sep = ",";
            }
            os << "}";
        };
        std::string sep;
        for (const auto &map : maps)
        {
            os << sep
               << "Maps::MapMetadata"
               << "(\"" << map.filename << "\""
               << ",\"" << map.hash << "\""
               << ",\"" << map.openbwHash << "\""
               << ",";
            firstSeedLayer(map.seeds);
            os << ")";
            sep = ",\n";
        }

        std::cout << os.str() << std::endl;
    }

    void initializeTest(BWTest &test, const Maps::MapMetadata &mapMetadata)
    {
        test.map = std::make_shared<Maps::MapMetadata>(mapMetadata);
        test.myModule = []()
        {
            return new DoNothingModule();
        };
        test.opponentModule = []()
        {
            return new DoNothingModule();
        };
        test.opponentRace = BWAPI::Races::Random;
        test.frameLimit = 10;
        test.expectWin = false;
        test.writeReplay = false;
    }
}

TEST(PopulateMapMetadata, OpenBwHashes)
{
    auto getHash = [](Maps::MapMetadata &mapMetadata)
    {
        BWTest test;
        initializeTest(test, mapMetadata);

        test.onStartMine = [&mapMetadata]()
        {
            mapMetadata.openbwHash = BWAPI::Broodwar->mapHash();
        };

        test.run();
    };
    auto maps = Maps::Get();
    for (auto &map : maps)
    {
        if (map.openbwHash.empty()) getHash(map);
    }

    out(maps);
}

TEST(PopulateMapMetadata, Seeds)
{
    auto missingSeeds = [](const Maps::MapMetadata &mapMetadata)
    {
        int total = 0;
        int missing = 0;
        for (int i = 0; i < mapMetadata.seeds.size(); i++)
        {
            for (int j = 0; j < mapMetadata.seeds[i].size(); j++)
            {
                if (i == j) continue;
                for (const auto &seed : mapMetadata.seeds[i][j])
                {
                    total++;
                    if (seed == 0) missing++;
                }
            }
        }

        if (missing == 0) return false;
        std::cout << mapMetadata.shortname() << " missing " << missing << " of " << total << std::endl;
        return true;
    };

    auto addSeed = [](Maps::MapMetadata &mapMetadata)
    {
        BWTest test;
        initializeTest(test, mapMetadata);

        bool foundNewSeed = false;
        test.onStartMine = [&]()
        {
            BWAPI::Broodwar->enableFlag(BWAPI::Flag::CompleteMapInformation);

        };
        test.onFrameMine = [&]()
        {
            if (BWAPI::Broodwar->getFrameCount() != 2) return;

            BWAPI::TilePosition enemyStartLocation;
            BWAPI::Race enemyRace;
            for (auto unit : BWAPI::Broodwar->enemy()->getUnits())
            {
                if (unit->getType().isResourceDepot())
                {
                    enemyStartLocation = unit->getTilePosition();
                    enemyRace = unit->getPlayer()->getRace();
                }
            }

            auto getStartPositionIndex = [](const BWAPI::TilePosition &startPosition)
            {
                int i = 0;
                for (const auto &tile : BWAPI::Broodwar->getStartLocations())
                {
                    if (tile == startPosition) return i;
                    i++;
                }
                throw std::invalid_argument("Provided tile is not in list of start positions");
            };

            int ourIndex = getStartPositionIndex(BWAPI::Broodwar->self()->getStartLocation());
            int enemyIndex = getStartPositionIndex(enemyStartLocation);
            int enemyRaceIndex = 0;
            if (enemyRace == BWAPI::Races::Terran) enemyRaceIndex = 1;
            if (enemyRace == BWAPI::Races::Protoss) enemyRaceIndex = 2;

            int currentValue = mapMetadata.seeds[ourIndex][enemyIndex][enemyRaceIndex];
            if (currentValue == 0)
            {
                mapMetadata.seeds[ourIndex][enemyIndex][enemyRaceIndex] = test.randomSeed;
                foundNewSeed = true;
            }
        };

        test.run();

        return foundNewSeed;
    };

    auto maps = Maps::Get();
    int countSinceLastOutput = 0;
    for (auto &map : maps)
    {
        while (missingSeeds(map))
        {
            countSinceLastOutput++;
            if (addSeed(map) || (countSinceLastOutput % 20) == 0)
            {
                out(maps);
                countSinceLastOutput = 0;
            }
        }
    }
}

TEST(PopulateMapMetadata, TestSeeds)
{
    Maps::RunOnEachStartLocationPair(Maps::Get("VermeerSE_2.1"), [](BWTest test)
    {
        test.myModule = []()
        {
            return new DoNothingModule();
        };
        test.opponentModule = []()
        {
            return new DoNothingModule();
        };
        test.opponentRace = BWAPI::Races::Random;
        test.frameLimit = 10;
        test.expectWin = false;
        test.writeReplay = true;
        test.run();
    });
}
