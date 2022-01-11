#include "gtest/gtest.h"

#include "BWAPI/GameImpl.h"
#include "BW/BWData.h"

#include <filesystem>

namespace {
    int getReplaySeed(const std::string &replayFilename)
    {
        BW::GameOwner gameOwner;
        BWAPI::BroodwarImpl_handle h(gameOwner.getGame());
        BWAPI::BroodwarImpl.bwgame.setMapFileName(replayFilename);
        h->createSinglePlayerGame([&](){});
        return h->getRandomSeed();
    }
}

TEST(Replays, GetReplaySeeds)
{
    std::set<int> allSeeds;

    std::string path = "/Users/bmnielsen/Downloads/Replays";
    for (const auto & entry : std::filesystem::directory_iterator(path))
    {
        int seed = getReplaySeed(entry.path());
        auto result = allSeeds.insert(seed);
        if (!result.second)
        {
            std::cout << "Dupe!" << std::endl;
        }
    }
}
