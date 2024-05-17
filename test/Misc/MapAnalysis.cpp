#include <utility>

#include "BWTest.h"
#include "DoNothingModule.h"

namespace
{
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

TEST(MapAnalysis, BuildMapMetadata)
{
    std::vector<std::pair<std::string, std::string>> mapFilenamesAndHashes = {
        {"maps/aiide2023/(2)CarthageIII_3.0.scx", "fe0a18342c797203d585dc2a3241f3974b765e0b"},
        {"maps/aiide2023/(2)ChupungRyeong_2.1.scx", "f391700c3551e145852822ff95e27edd3173fae6"},
        {"maps/aiide2023/(2)Crossing_Field_1.34.scx", "97944269ea55365d13c310f46c9337f5e873dc6c"},
        {"maps/aiide2023/(2)Destination.scx", "4e24f217d2fe4dbfa6799bc57f74d8dc939d425b"},
        {"maps/aiide2023/(2)EclipseBW_1.16.1.scx", "3cf2b26da33b8e798b0e359cd621569b5725124d"},
        {"maps/aiide2023/(2)HeartbreakRidge.scx", "6f8da3c3cc8d08d9cf882700efa049280aedca8c"},
        {"maps/aiide2023/(2)MatchPoint1.3.scx", "8b36cbc2df21bc628dd04e623408950638cdcbde"},
        {"maps/aiide2023/(2)NewBloodyRidge_2.0.scx", "30f8396f0399403ddf60fc733ce99f393602575e"},
        {"maps/aiide2023/(2)OddEyeIII_3.0.scx", "a4689419e88bb865e9003ba55d2913a38f3cf5d2"},
        {"maps/aiide2023/(2)Overwatch_2.2.scx", "630fc57ab58f4ac8a28d2b8dcd1cf524fd1566ec"},
        {"maps/aiide2023/(2)PolarisRhapsody_1.0.scx", "614d0048c6cc9dcf08da1409462f22f2ac4f5a0b"},
        {"maps/aiide2023/(3)Demian_1.0.scx", "9b7b1f03fd03baa6e4310cea71fa5eab82e9af5d"},
        {"maps/aiide2023/(3)Longinus2.scx", "d16719e736252d77fdbb0d8405f7879f564bfe56"},
        {"maps/aiide2023/(3)NeoAztec_2.0_KeSPA.scx", "b20fc580f0743f61d0d4ef1a56054505a5c63217"},
        {"maps/aiide2023/(3)NeoMoonGlaive_2.1_KeSPA.scx", "ed37fce47d1312e39a3a379c122382790e190cbd"},
        {"maps/aiide2023/(3)NeoSylphid_2.0.scx", "dbd844012e678b23ca8ef21b3b62008589a554b5"},
        {"maps/aiide2023/(3)PowerBond_1.00.scx", "731138b5b844a4a0b4a4bb4e495969fd6659414c"},
        {"maps/aiide2023/(4)Allegro_1.1.scx", "e3a3ff717eacb1af111aab9121004d9c0e88f074"},
        {"maps/aiide2023/(4)ArcadiaII_2.02.scx", "442e456721c94fd085ecd10230542960d57928d9"},
        {"maps/aiide2023/(4)Beltway_1.1_KeSPA.scx", "7f711be2ca57e8bdae57eec945eeb432c67ca561"},
        {"maps/aiide2023/(4)ByzantiumII_2.3.scx", "28062968c363950f15410190a3f06158f84ac944"},
        {"maps/aiide2023/(4)CircuitBreaker.scx", "450a792de0e544b51af5de578061cb8a2f020f32"},
        {"maps/aiide2023/(4)ColosseumII_2.0.scx", "bde51d09a2ad733db9e5492354798045db2ced3e"},
        {"maps/aiide2023/(4)DantesPeakSE_2.0.scx", "cb41ae895d66b71bbd54812fdef370f0e235cc38"},
        {"maps/aiide2023/(4)Eddy_1.02.scx", "3078ee93e4a0c3c2ad22c73ab62ef806d9436c3d"},
        {"maps/aiide2023/(4)EyeOfTheStorm_1.0_iCCup.scx", "5e0bc3346c201ede3c92d6e970ffd4e8bab8b0cf"},
        {"maps/aiide2023/(4)FightingSpirit_1.3.scx", "d2f5633cc4bb0fca13cd1250729d5530c82c7451"},
        {"maps/aiide2023/(4)Gladiator_1.1.scx", "798bea3acce68788ff1b32d9d777ba7a12c883a1"},
        {"maps/aiide2023/(4)GodsGarden_1.0_KeSPA.scx", "2acb8e8cc2ec9b0911a73f2f29dcce424862dddd"},
        {"maps/aiide2023/(4)GrandLineSE_2.2_KeSPA.scx", "e909d176390f5115e13c449497cc9e52805ac384"},
        {"maps/aiide2023/(4)JudgmentDay_1.2.scx", "2f69eaa1a73bb743934d55e7ea12186fe340e656"},
        {"maps/aiide2023/(4)KatrinaSE_2.0.scx", "444b805a88f971c6b2c5f8d2a467de3c1fb2f001"},
        {"maps/aiide2023/(4)La Mancha1.1.scx", "39bf400dbb3ee3b5d6710e8ca410c727c4636560"},
        {"maps/aiide2023/(4)Largo_1.5.scx", "4176c3dd9e8cd11d3da1323b4c17e059d06a8fed"},
        {"maps/aiide2023/(4)NeoGroundZero_2.0_KeSPA.scx", "29f2c1ccb2f7dbee81d1c2387289471cdcb83cd9"},
        {"maps/aiide2023/(4)NeoJade_2.0_KeSPA.scx", "64e53c70a4036b42f6480f0fc9530b38789159c4"},
        {"maps/aiide2023/(4)NewEmpireOfTheSun_2.0.scm", "2237701cb9bfaa2600d4aa4bcdc6449a199c81cf"},
        {"maps/aiide2023/(4)NewSniperRidge_2.0_KeSPA.scx", "9e9e6a3372251ac7b0acabcf5d041fbf0b755fdb"},
        {"maps/aiide2023/(4)NewWindAndCloud_2.2.scm", "e72a0a853545b77b39229a8c11d5c5624c09242b"},
        {"maps/aiide2023/(4)Overwatch_1.3.scx", "75ded6b606db6546c46789a42c11cebe58cdab4b"},
        {"maps/aiide2023/(4)PamirPlateau_1.2.scm", "75ded6b606db6546c46789a42c11cebe58cdab4b"},
        {"maps/aiide2023/(4)Polypoid_1.65.scx", "ad870839912421dc3b4fd736a954bf770693ba9a"},
        {"maps/aiide2023/(4)Python.scx", "de2ada75fbc741cfa261ee467bf6416b10f9e301"},
        {"maps/aiide2023/(4)Roadkill.scm", "5386ec02cc3ee913acc55181896c287ae9d5b5c6"},
        {"maps/aiide2023/(4)VermeerSE_2.1.scm", "90b6f33ba4f24a67ee875c18cc4a52ee490ce26a)"}
    };

    std::vector<Maps::MapMetadata> maps;
    for (const auto &[mapFilename, mapHash] : mapFilenamesAndHashes)
    {
        Maps::MapMetadata map(mapFilename, mapHash, "", {});

        size_t startLocationCount = -1;
        std::map<BWAPI::TilePosition, int> startPositionToSeed;

        while (startLocationCount == -1 || startPositionToSeed.size() < startLocationCount)
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
            test.map = std::make_shared<Maps::MapMetadata>(map);
            test.frameLimit = 10;
            test.expectWin = false;
            test.writeReplay = false;

            test.onStartMine = [&]()
            {
                map.openbwHash = BWAPI::Broodwar->mapHash();
                startLocationCount = BWAPI::Broodwar->getStartLocations().size();
                auto startLocationTile = BWAPI::Broodwar->self()->getStartLocation();
                if (!startPositionToSeed.contains(startLocationTile))
                {
                    startPositionToSeed[startLocationTile] = test.randomSeed;
                }
            };

            test.run();
        }

        std::vector<int> startPositionSeeds;
        for (auto &pair : startPositionToSeed)
        {
            startPositionSeeds.emplace_back(pair.second);
        }
        map.startLocationSeeds = startPositionSeeds;

        maps.push_back(map);
    }

    for (auto &map : maps)
    {
        std::ostringstream ss;
        ss << "Maps::MapMetadata(\"" << map.filename << "\",\n";
        ss << "\"" << map.hash << "\",\n";
        ss << "\"" << map.openbwHash << "\",\n";
        ss << "{";
        bool first = true;
        for (auto &seed : map.startLocationSeeds)
        {
            if (!first) ss << ", ";
            first = false;
            ss << seed;
        }
        ss << "}),";
        std::cout << ss.str() << std::endl;
    }
}