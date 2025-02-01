#pragma once

#include <vector>
#include <string>
#include <algorithm>
#include <memory>
#include <functional>
#include <sstream>

struct BWTest;

namespace Maps
{
    namespace
    {
        int playersFromFilename(const std::string &filename)
        {
            auto exception = [&filename](const std::string &reason)
            {
                std::ostringstream text;
                text << "Cannot get players from filename; " << reason << ". Filename: " << filename;
                return std::invalid_argument(text.str());
            };

            auto shortFilename = filename.substr(filename.rfind('/') + 1);
            if (shortFilename.size() < 2) throw exception("filename too short");

            if (shortFilename[1] == '2') return 2;
            if (shortFilename[1] == '3') return 3;
            if (shortFilename[1] == '4') return 4;
            throw exception("second character in short filename is not 2-4");
        }
    }
    struct MapMetadata
    {
        explicit MapMetadata(const std::string &filename) : filename(filename), players(playersFromFilename(filename))
        {
            initializeSeeds();
        }

        MapMetadata(const std::string &filename, std::string hash, std::string openbwHash)
                : filename(filename)
                , players(playersFromFilename(filename))
                , hash(std::move(hash))
                , openbwHash(std::move(openbwHash))
        {
            initializeSeeds();
        }

        MapMetadata(const std::string &filename, std::string hash, std::string openbwHash, std::vector<std::vector<std::array<int, 3>>> seeds)
                : filename(filename)
                , players(playersFromFilename(filename))
                , hash(std::move(hash))
                , openbwHash(std::move(openbwHash))
                , seeds(std::move(seeds)) {}

        std::string filename;
        int players;

        std::string hash;
        std::string openbwHash;

        // First layer: my start location
        // Second layer: enemy start location
        // Final layer: enemy race, 0=Zerg, 1=Terran, 2=Protoss
        std::vector<std::vector<std::array<int, 3>>> seeds;

        [[nodiscard]] std::string shortfilename() const
        {
            return filename.substr(filename.rfind('/') + 1);
        }

        [[nodiscard]] std::string shortname() const
        {
            std::string result = shortfilename();
            std::replace(result.begin(), result.end(), ' ', '_');
            return result.substr(0, result.rfind('.'));
        }

    private:
        void initializeSeeds()
        {
            seeds.resize(players);
            for (int i = 0; i < players; i++)
            {
                seeds[i].resize(players);
            }
        }
    };

    std::vector<MapMetadata> Get(const std::string &search = "", int players = 0);

    std::shared_ptr<MapMetadata> GetOne(const std::string &search = "", int players = 0);

    void RunOnEach(const std::vector<MapMetadata> &maps, const std::function<void(BWTest)> &runner);

    void RunOnEachStartLocation(const std::vector<MapMetadata> &maps, const std::function<void(BWTest)> &runner);

    void RunOnEachStartLocationPair(const std::vector<MapMetadata> &maps, const std::function<void(BWTest)> &runner);

    void RunOnEachStartLocationPairAndRandomRace(const std::vector<MapMetadata> &maps, const std::function<void(BWTest)> &runner);
}
