#include "Serialization.h"

#include <bitsery/adapter/buffer.h>
#include <bitsery/adapter/stream.h>
#include <bitsery/traits/vector.h>
#include <bitsery/ext/std_map.h>
#include <bitsery/ext/std_set.h>

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
            auto resourceToRootNodesSerializer = [&]<typename T>(
                    S &s,
                    std::unordered_map<TilePosition, std::unordered_map<PositionAndVelocity, SerializedPath<T>>> &value)
            {
                s.ext(value, bitsery::ext::StdMap{INT_MAX},
                      [&](S& s, TilePosition& key, std::unordered_map<PositionAndVelocity, SerializedPath<T>>& v) {
                    s.object(key);
                    s.ext(v, bitsery::ext::StdMap{INT_MAX}, [&](S& s, PositionAndVelocity& key, SerializedPath<T>& v) {
                        s.object(v);
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
            ser.container(data.tenDistanceAndResendAlwaysArrives, 256, [&](S &s, std::pair<uint8_t, uint8_t> &v) {
                s.value1b(v.first);
                s.value1b(v.second);
            });
            if constexpr (!serializing) data.tenDistanceAndResendAlwaysArrives.shrink_to_fit();
            ser.value4b(data.minimumNextPathLength);
            ser.object(data.resourceToSerializedGatherPaths, resourceToRootNodesSerializer);
            ser.object(data.resourceToSerializedReturnPaths, resourceToRootNodesSerializer);

            auto initialSplitRotationSerializer = [](S &s, InitialSplitRotation &v)
            {
                s.value1b(v.delayFrames);
                s.ext(v.resendFrames, bitsery::ext::StdSet{INT_MAX}, [&](S& s, uint16_t &v) {
                    s.value2b(v);
                });
                s.value2b(v.gatherArrivalFrame);
                s.value2b(v.gatherActionFrame);
                s.value2b(v.returnArrivalFrame);
                s.ext(v.returnActionFramesToOccurrences, bitsery::ext::StdMap{INT_MAX}, [&](S& s, uint16_t &key, uint8_t &val) {
                    s.value2b(key);
                    s.value1b(val);
                });
            };
            auto initialSplitDataSerializer = [&initialSplitRotationSerializer](
                    S &s,
                    std::unordered_map<PositionAndVelocity, std::map<std::pair<TilePosition, TilePosition>, InitialSplitData>> &v)
            {
                s.ext(v, bitsery::ext::StdMap{INT_MAX}, [&](S &s,
                                                            PositionAndVelocity &key,
                                                            std::map<std::pair<TilePosition, TilePosition>, InitialSplitData> &v)
                {
                    s.object(key);
                    s.ext(v, bitsery::ext::StdMap{INT_MAX}, [&](S &s,
                                                                std::pair<TilePosition, TilePosition> &key,
                                                                InitialSplitData &v)
                    {
                        s.object(key.first);
                        s.object(key.second);
                        s.object(v.firstRotation, initialSplitRotationSerializer);
                        s.ext(v.firstRotationDeliveryToSecondRotation, bitsery::ext::StdMap{INT_MAX}, [&](S &s,
                                                                                                          uint16_t &key,
                                                                                                          InitialSplitRotation &v)
                        {
                            s.value2b(key);
                            s.object(v, initialSplitRotationSerializer);
                        });
                    });
                });
            };
            ser.object(data.startLocationToPatchPairToInitialSplitDataZerg, initialSplitDataSerializer);
            ser.object(data.startLocationToPatchPairToInitialSplitDataNotZerg, initialSplitDataSerializer);
            ser.object(data.startLocationToPatchPairToInitialSplitDataUnknown, initialSplitDataSerializer);

            ser.ext(data.resourceToAverageSingleWorkerRotationTime,
                    bitsery::ext::StdMap{INT_MAX},
                    [&](S &s,
                        TilePosition &key,
                        uint8_t &v)
                    {
                        s.object(key);
                        s.value1b(v);
                    });
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
