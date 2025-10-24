#include "Serialization.h"

#include <bitsery/adapter/buffer.h>
#include <bitsery/adapter/stream.h>
#include <bitsery/traits/vector.h>
#include <bitsery/ext/std_map.h>

#include <zstdstream.h>

#include <filesystem>

#include "FileTools.h"

// Provides the best speed (on my machine) for the training use case where we are continually saving and reloading
// Lower levels are not much faster to compress and slower to decompress (because of increased time to read the larger files)
// Higher levels do not compress the files sufficiently better to make up for the increased compression time
#define COMPRESSION_LEVEL 4

namespace MiningOptimizationTraining::Serialization
{
    namespace
    {
        bool gameParametersInitialized = false;
        std::string mapHash;

        void ensureGameParametersInitialized()
        {
            if (gameParametersInitialized) return;

            mapHash = BWAPI::Broodwar->mapHash();
            gameParametersInitialized = true;
        }

        std::string getFilename(bool writing)
        {
            std::ostringstream builder;
            builder << "mining-optimization-training";
            builder << "_" << mapHash;
            return FileTools::getFilePath(builder.str(), "bin.zstd", writing);
        }

        template <typename S>
        void serialize(S &ser, MapData &data)
        {
            std::function<void(S&, GatherArrivalObservations&)> gatherArrivalObservationsSerializer;
            std::function<void(S&, GatherObservations&)> gatherObservationsSerializer;
            std::function<void(S&, std::unordered_map<TilePosition, std::unordered_map<PositionOnPath, GatherObservations>>&)>
                resourceToGatherRootNodesSerializer;

            gatherArrivalObservationsSerializer = [](S &s, GatherArrivalObservations &value)
            {
                s.ext(value.arrivalToOccurrences, bitsery::ext::StdMap{INT_MAX}, [](S& s, ArrivalData& key, uint32_t& v) {
                    s.object(key);
                    s.value4b(v);
                });
                s.value4b(value.collisions);
                s.value4b(value.nonCollisions);
            };

            gatherObservationsSerializer = [&](S &s, GatherObservations &value)
            {
                s.object(value.pos);
                s.value4b(value.occurrences);
                s.object(value.arrivalObservations, gatherArrivalObservationsSerializer);
                s.object(value.arrivalObservationsAfterResend, gatherArrivalObservationsSerializer);
                s.container(value.nextPositions, INT_MAX, [&](S &s, GatherObservations &v) {
                    s.object(v, gatherObservationsSerializer);
                });
                s.container(value.nextPositionsAfterResend, INT_MAX, [&](S &s, GatherObservations &v) {
                    s.object(v, gatherObservationsSerializer);
                });
            };

            resourceToGatherRootNodesSerializer = [&](
                    S &s,
                    std::unordered_map<TilePosition, std::unordered_map<PositionOnPath, GatherObservations>> &value)
            {
                s.ext(value, bitsery::ext::StdMap{INT_MAX}, [&](S& s, TilePosition& key, std::unordered_map<PositionOnPath, GatherObservations>& v) {
                    s.object(key);
                    s.ext(v, bitsery::ext::StdMap{INT_MAX}, [&](S& s, PositionOnPath& key, GatherObservations& v) {
                        s.object(key);
                        s.object(v, gatherObservationsSerializer);
                    });
                });
            };

            ser.object(data.resourceToGatherRootNodes, resourceToGatherRootNodesSerializer);
        }
    }

    void setGameParameters(const std::string &_mapHash)
    {
        mapHash = _mapHash;
        gameParametersInitialized = true;
    }

    void readMapData(MapData &data)
    {
        ensureGameParametersInitialized();

        // Don't need to reload the data if the map hasn't changed
        if (!data.mapHash.empty() && data.mapHash == mapHash)
        {
            Log::Get() << "Using already-loaded mining optimization data";
            return;
        }

        data.clear(mapHash);

        auto filename = getFilename(false);
        if (filename.empty())
        {
            Log::Get() << "No saved mining optimization data available";
            return;
        }

        zstd::ifstream file(filename);
        if (!file.good())
        {
            Log::Get() << "Could not open saved mining optimization data from " << filename;
            return;
        }

        bitsery::Deserializer<bitsery::InputStreamAdapter> ser{file};
        serialize(ser, data);
        file.close();

        Log::Get() << "Read mining optimization data from " << filename;
    }

    void writeMapData(MapData &data)
    {
        ensureGameParametersInitialized();

        auto filename = getFilename(true);
        if (filename.empty())
        {
            Log::Get() << "ERROR: Could not generate filename for mining optimization data";
            return;
        }

        zstd::ofstream file(filename, std::ofstream::trunc, COMPRESSION_LEVEL);
        if (file.fail())
        {
            Log::Get() << "Could not open mining optimization data file for writing: " << filename;
            return;
        }

        bitsery::Serializer<bitsery::OutputStreamAdapter> ser{file};
        serialize(ser, data);
        ser.adapter().flush();
        file.close();

        Log::Get() << "Wrote mining optimization data to " << filename;
    }
}
