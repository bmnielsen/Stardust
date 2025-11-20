#include "Serialization.h"

#include <bitsery/adapter/buffer.h>
#include <bitsery/adapter/stream.h>
#include <bitsery/traits/vector.h>
#include <bitsery/ext/std_map.h>

#include <zstdstream.h>

#include <filesystem>

#include "FileTools.h"

// Serialization logic for mining optimization data
// We use the following techniques to reduce the data file size:
// - Positions are stored as deltas instead of absolute positions wherever possible
// - The heading and velocity are only included on positions where they are needed to discriminate between peers
// - The vectors and maps with occurrence rates are serialized such that they don't need a separate size item
// - We know we never have resend data if we don't have non-resend data
// - We use maximum zstd compression
namespace MiningOptimization::Serialization
{
    namespace
    {
        bool gameParametersInitialized = false;
        std::string mapHash;

        uint8_t zero = 0;

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

        template <bool serializing, typename S>
        void serialize(S &ser, MapData &data)
        {
            auto rootNodeSerializer = [&]<typename T>(S &s, Path<T>& value)
            {
                std::function<void(S&, PathNode<T>&)> pathNodeSerializer;

                auto customVectorSerializer = [&]()
                {
                    if constexpr (serializing)
                    {
                        return [&]<typename U>(S &s, std::vector<std::pair<U, uint8_t>> &vec)
                        {
                            // If the vector is empty, just write a zero
                            if (vec.empty())
                            {
                                s.value1b(zero);
                                return;
                            }

                            // Write the occurrences before the nodes
                            for (auto &[k, v] : vec)
                            {
                                s.value1b(v);
                                if constexpr (std::is_same_v<U, PathNode<T>>)
                                {
                                    s.object(k, pathNodeSerializer);
                                }
                                else
                                {
                                    s.object(k);
                                }
                            }
                        };
                    }
                    else
                    {
                        return [&]<typename U>(S &s, std::vector<std::pair<U, uint8_t>> &vec)
                        {
                            uint8_t total = 0;
                            while (total < 255)
                            {
                                uint8_t occurrenceRate;
                                s.value1b(occurrenceRate);

                                // First item will be zero for an empty vector
                                if (occurrenceRate == 0) return;

                                U item;
                                if constexpr (std::is_same_v<U, PathNode<T>>)
                                {
                                    s.object(item, pathNodeSerializer);
                                }
                                else
                                {
                                    s.object(item);
                                }

                                vec.emplace_back(std::move(item), occurrenceRate);

                                total += occurrenceRate;
                            }
                            vec.shrink_to_fit();
                        };
                    }
                }();

                pathNodeSerializer = [&](S &s, PathNode<T>& value)
                {
                    s.object(value.pos);
                    s.object(value.arrivalData, customVectorSerializer);
                    if (!value.arrivalData.empty()) s.object(value.arrivalDataAfterResend, customVectorSerializer);
                    s.object(value.nextPositions, customVectorSerializer);
                    if (!value.nextPositions.empty()) s.object(value.nextPositionsAfterResend, customVectorSerializer);
                };

                s.object(value.pos);
                s.object(value.nextPositions, customVectorSerializer);
            };

            auto resourceToRootNodesSerializer = [&]<typename T>(
                    S &s,
                    std::unordered_map<TilePosition, std::unordered_map<PositionAndVelocity, Path<T>>> &value)
            {
                s.ext(value, bitsery::ext::StdMap{INT_MAX}, [&](S& s, TilePosition& key, std::unordered_map<PositionAndVelocity, Path<T>>& v) {
                    s.object(key);
                    s.ext(v, bitsery::ext::StdMap{INT_MAX}, [&](S& s, PositionAndVelocity& key, Path<T>& v) {
                        s.object(v, rootNodeSerializer);
                        if constexpr (!serializing)
                        {
                            key = v.pos;
                        }
                    });
                });
            };

            ser.container(data.positionDeltas, 128, [&](S &s, std::pair<int8_t, int8_t> &v) {
                s.value1b(v.first);
                s.value1b(v.second);
            });
            if constexpr (!serializing) data.positionDeltas.shrink_to_fit();
            ser.value4b(data.minimumNextPathLength);
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
        serialize<false>(ser, data);
        file.close();

        Log::Get() << "Read mining optimization data from " << filename;
    }

    void writeMapData(MapData &data)
    {
        gameParametersInitialized = true;
        mapHash = data.mapHash;

        auto filename = getFilename(true);
        if (filename.empty())
        {
            Log::Get() << "ERROR: Could not generate filename for mining optimization data";
            return;
        }

        zstd::ofstream file(filename, std::ofstream::trunc, ZSTD_maxCLevel());
        if (file.fail())
        {
            Log::Get() << "Could not open mining optimization data file for writing: " << filename;
            return;
        }

        bitsery::Serializer<bitsery::OutputStreamAdapter> ser{file};
        serialize<true>(ser, data);
        ser.adapter().flush();
        file.close();

        Log::Get() << "Wrote mining optimization data to " << filename;
    }
}
