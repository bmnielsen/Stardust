#pragma once

#include <vector>
#include <string>

struct BWTest;

namespace Maps
{
    struct MapMetadata
    {
        MapMetadata(std::string filename, std::string hash, std::string openbwHash, std::vector<int> startLocationSeeds)
                : filename(std::move(filename))
                , hash(std::move(hash))
                , openbwHash(std::move(openbwHash))
                , startLocationSeeds(std::move(startLocationSeeds)) {}

        std::string filename;
        std::string hash;
        std::string openbwHash;
        std::vector<int> startLocationSeeds;

        std::string shortname()
        {
            return filename.substr(filename.rfind('/') + 1);
        }
    };

    std::vector<MapMetadata> Get(const std::string &search = "", int players = 0);

    std::shared_ptr<MapMetadata> GetOne(const std::string &search = "", int players = 0);

    void RunOnEach(const std::vector<MapMetadata> &maps, const std::function<void(BWTest)> &runner);

    void RunOnEachStartLocation(const std::vector<MapMetadata> &maps, const std::function<void(BWTest)> &runner);
}
