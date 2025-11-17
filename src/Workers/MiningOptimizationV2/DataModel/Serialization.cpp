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

//#define COMPRESSION_LEVEL ZSTD_maxCLevel()

namespace MiningOptimizationV2::Serialization
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
            builder << "mining-optimization";
            builder << "_" << mapHash;
            return FileTools::getFilePath(builder.str(), "bin.zstd", writing);
        }

        template <typename S>
        void serialize(S &ser, MapData &data)
        {
            auto rootNodeSerializer = [&]<typename T>(S &s, Path<T>& value)
            {
                std::function<void(S&, PathNode<T>&)> pathNodeSerializer;

                pathNodeSerializer = [&](S &s, PathNode<T>& value)
                {
                    s.object(value.pos);
                    s.ext(value.arrivalData, bitsery::ext::StdMap{INT_MAX}, [](auto &s, T &key, uint8_t &value)
                    {
                        s.object(key);
                        s.value1b(value);
                    });
                    s.ext(value.arrivalDataAfterResend, bitsery::ext::StdMap{INT_MAX}, [](auto &s, T &key, uint8_t &value)
                    {
                        s.object(key);
                        s.value1b(value);
                    });
                    s.container(value.nextPositions, INT_MAX, [&](S &s, std::pair<PathNode<T>, uint8_t> &v) {
                        s.object(v.first, pathNodeSerializer);
                        s.value1b(v.second);
                    });
                    s.container(value.nextPositionsAfterResend, INT_MAX, [&](S &s, std::pair<PathNode<T>, uint8_t> &v) {
                        s.object(v.first, pathNodeSerializer);
                        s.value1b(v.second);
                    });
                };

                s.object(value.pos);
                s.container(value.nextPositions, INT_MAX, [&](S &s, std::pair<PathNode<T>, uint8_t> &v) {
                    s.object(v.first, pathNodeSerializer);
                    s.value1b(v.second);
                });
            };

            auto resourceToRootNodesSerializer = [&]<typename T>(
                    S &s,
                    std::unordered_map<TilePosition, std::unordered_map<PositionAndVelocity, Path<T>>> &value)
            {
                s.ext(value, bitsery::ext::StdMap{INT_MAX}, [&](S& s, TilePosition& key, std::unordered_map<PositionAndVelocity, Path<T>>& v) {
                    s.object(key);
                    s.ext(v, bitsery::ext::StdMap{INT_MAX}, [&](S& s, PositionAndVelocity& key, Path<T>& v) {
                        s.object(key);
                        s.object(v, rootNodeSerializer);
                    });
                });
            };

            ser.container(data.positionDeltas, 128, [&](S &s, std::pair<uint8_t, uint8_t> &v) {
                s.value1b(v.first);
                s.value1b(v.second);
            });

            ser.object(data.resourceToGatherPaths, resourceToRootNodesSerializer);
            ser.object(data.resourceToReturnPaths, resourceToRootNodesSerializer);
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
