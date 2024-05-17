#pragma once

#include <vector>
#include <string>
#include <algorithm>
#include <memory>
#include <functional>

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

        std::string shortfilename()
        {
            return filename.substr(filename.rfind('/') + 1);
        }

        std::string shortname()
        {
            std::string result = shortfilename();
            std::replace(result.begin(), result.end(), ' ', '_');
            return result.substr(0, result.rfind('.'));
        }
    };

    std::vector<MapMetadata> Get(const std::string &search = "", int players = 0);

    std::shared_ptr<MapMetadata> GetOne(const std::string &search = "", int players = 0);

    void RunOnEach(const std::vector<MapMetadata> &maps, const std::function<void(BWTest)> &runner);

    void RunOnEachStartLocation(const std::vector<MapMetadata> &maps, const std::function<void(BWTest)> &runner);
}
