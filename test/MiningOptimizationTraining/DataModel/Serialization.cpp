#include "Serialization.h"

#include <bitsery/adapter/buffer.h>
#include <bitsery/adapter/stream.h>
#include <bitsery/traits/vector.h>
#include <bitsery/ext/std_map.h>
#include <bitsery/ext/std_optional.h>
#include <bitsery/ext/std_set.h>

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

        std::string getFilename(const std::string &dataset, bool writing)
        {
            std::ostringstream builder;
            builder << dataset << "_" << mapHash;
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
                    s.value1b(value.type);
                    s.ext(value.arrivalData, bitsery::ext::StdMap{INT_MAX}, [](auto &s, T &key, uint32_t &value)
                    {
                        s.object(key);
                        s.value4b(value);
                    });
                    s.ext(value.arrivalDataAfterResend, bitsery::ext::StdMap{INT_MAX}, [](auto &s, T &key, uint32_t &value)
                    {
                        s.object(key);
                        s.value4b(value);
                    });
                    s.container(value.nextPositions, INT_MAX, [&](S &s, std::pair<PathNode<T>, uint32_t> &v) {
                        s.object(v.first, pathNodeSerializer);
                        s.value4b(v.second);
                    });
                    s.container(value.nextPositionsAfterResend, INT_MAX, [&](S &s, std::pair<PathNode<T>, uint32_t> &v) {
                        s.object(v.first, pathNodeSerializer);
                        s.value4b(v.second);
                    });
                };

                s.object(value.pos);

                s.ext(value.nextPositions, bitsery::ext::StdMap{INT_MAX},
                      [&](S &s, MiningOptimization::CannonPlacement &key, std::vector<std::pair<PathNode<T>, uint32_t>> &vec)
                      {
                          s.object(key);
                          s.container(vec, INT_MAX, [&](S &s, std::pair<PathNode<T>, uint32_t> &v)
                          {
                              s.object(v.first, pathNodeSerializer);
                              s.value4b(v.second);
                          });
                      });
                s.value4b(value.timesExploredWithNoCollision);
                s.value4b(value.timesExploredWithCollision);
                s.ext(value.bestArrivalDelaysAndOccurrences, bitsery::ext::StdMap{INT_MAX}, [&](S& s, uint16_t& key, uint32_t& v) {
                    s.value2b(key);
                    s.value4b(v);
                });
                s.container(value.positionsToExplore, INT_MAX, [&](S &s, BWAPI::ExactPosition &v) {
                    s.value4b(v.x);
                    s.value4b(v.y);
                    s.value1b(v.heading);
                    v.velocityX = 0;
                    v.velocityY = 0;
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

            ser.object(data.resourceToGatherPaths, resourceToRootNodesSerializer);
            ser.object(data.resourceToReturnPaths, resourceToRootNodesSerializer);
            ser.ext(data.resourceToReturnPathStartPositions, bitsery::ext::StdMap{INT_MAX},
                    [&](S& s, TilePosition& key, std::unordered_set<PositionAndVelocity>& v) {
                s.object(key);
                s.ext(v, bitsery::ext::StdSet{INT_MAX}, [&](S& s, PositionAndVelocity& value) {
                    s.object(value);
                });
            });
        }

        template <bool serializing, typename S>
        void serialize(S &ser, InitialWorkerMapData &data)
        {
            std::map<BWAPI::Position, std::vector<InitialWorkerMapData::OrderProcessTimerReset>> startingWorkerPositionToOrderProcessTimerReset;

            ser.ext(data.startingWorkerPositionToOrderProcessTimerReset, bitsery::ext::StdMap{INT_MAX},
                    [&](S& s, BWAPI::Position& key, std::vector<InitialWorkerMapData::OrderProcessTimerReset>& values) {
                s.value4b(key.x);
                s.value4b(key.y);
                s.container(values, INT_MAX, [&](S &s, InitialWorkerMapData::OrderProcessTimerReset &v) {
                    s.value1b(v.value);
                    s.value1b(v.opponentIsZerg);
                    s.value1b(v.opponentStartLocationsCount);
                    s.value4b(v.randomSeed);
                });
            });

            auto pathNodeSerializer = [&]<typename T>(S &s, InitialWorkerPathNode<T> &value)
            {
                std::function<void(S&, InitialWorkerPathNode<T>&)> pathNodeSerializerImpl;
                pathNodeSerializerImpl = [&](S &s, InitialWorkerPathNode<T> &value)
                {
                    // In theory this should have been handled by Bitsery's StdSharedPtr extension, but I couldn't get it to work
                    auto serializeNextPosition = [&](S &s, std::unique_ptr<InitialWorkerPathNode<T>> &value)
                    {
                        if constexpr(serializing)
                        {
                            s.boolValue(static_cast<bool>(value));
                            if (value)
                            {
                                s.object(*value, pathNodeSerializerImpl);
                            }
                        }
                        else
                        {
                            value = nullptr;
                            bool exists{};
                            s.boolValue(exists);
                            if (exists)
                            {
                                auto obj = new InitialWorkerPathNode<T>();
                                s.object(*obj, pathNodeSerializerImpl);
                                value.reset(obj);
                            }
                        }
                    };

                    s.object(value.pos);
                    s.value1b(value.type);
                    s.object(value.arrivalData);
                    s.ext(value.arrivalDataAfterResend, bitsery::ext::StdOptional{});
                    s.object(value.nextPosition, serializeNextPosition);
                    s.object(value.nextPositionAfterResend, serializeNextPosition);
                };
                pathNodeSerializerImpl(s, value);
            };

            ser.ext(data.startingWorkerPositionToPatchToFirstGatherPath,
                    bitsery::ext::StdMap{INT_MAX},
                    [&](S &s, BWAPI::ExactPosition &key, std::map<TilePosition, InitialWorkerGatherPathNode> &v)
                    {
                        s.object(key);
                        s.ext(v, bitsery::ext::StdMap{INT_MAX}, [&](S &s, TilePosition &key, InitialWorkerGatherPathNode &v)
                        {
                            s.object(key);
                            s.object(v, pathNodeSerializer);
                        });
                    });
            ser.ext(data.startingWorkerPositionToPatchesToSecondGatherPaths,
                    bitsery::ext::StdMap{INT_MAX},
                    [&](S &s,
                        BWAPI::ExactPosition &key,
                        std::map<std::pair<TilePosition, TilePosition>, std::map<BWAPI::ExactPosition, InitialWorkerGatherPathNode>> &v)
                    {
                        s.object(key);
                        s.ext(v,
                              bitsery::ext::StdMap{INT_MAX},
                              [&](S &s, std::pair<TilePosition, TilePosition> &key, std::map<BWAPI::ExactPosition, InitialWorkerGatherPathNode> &v)
                              {
                                  s.object(key.first);
                                  s.object(key.second);
                                  s.ext(v, bitsery::ext::StdMap{INT_MAX}, [&](S &s, BWAPI::ExactPosition &key, InitialWorkerGatherPathNode &v)
                                  {
                                      s.object(key);
                                      s.object(v, pathNodeSerializer);
                                  });
                              });
                    });
            ser.ext(data.startingWorkerPositionToPatchToReturnPaths,
                    bitsery::ext::StdMap{INT_MAX},
                    [&](S &s,
                        BWAPI::ExactPosition &key,
                        std::map<TilePosition, std::map<BWAPI::ExactPosition, InitialWorkerReturnPathNode>> &v)
                    {
                        s.object(key);
                        s.ext(v,
                              bitsery::ext::StdMap{INT_MAX},
                              [&](S &s, TilePosition &key, std::map<BWAPI::ExactPosition, InitialWorkerReturnPathNode> &v)
                              {
                                  s.object(key);
                                  s.ext(v, bitsery::ext::StdMap{INT_MAX}, [&](S &s, BWAPI::ExactPosition &key, InitialWorkerReturnPathNode &v)
                                  {
                                      s.object(key);
                                      s.object(v, pathNodeSerializer);
                                  });
                              });
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

        auto filename = getFilename("mining-optimization-training", false);
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

        auto filename = getFilename("mining-optimization-training", true);
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

    void readMapData(InitialWorkerMapData &data)
    {
        ensureGameParametersInitialized();

        // Don't need to reload the data if the map hasn't changed
        if (!data.mapHash.empty() && data.mapHash == mapHash)
        {
            Log::Get() << "Using already-loaded initial workers mining optimization data";
            return;
        }

        data.clear(mapHash);

        auto filename = getFilename("mining-optimization-training-initialworkers", false);
        if (filename.empty())
        {
            Log::Get() << "No saved initial workers mining optimization data available";
            return;
        }

        zstd::ifstream file(filename);
        if (!file.good())
        {
            Log::Get() << "Could not open saved initial workers mining optimization data from " << filename;
            return;
        }

        bitsery::Deserializer<bitsery::InputStreamAdapter> ser{file};
        serialize<false>(ser, data);
        file.close();

        Log::Get() << "Read initial workers mining optimization data from " << filename;
    }

    void writeMapData(InitialWorkerMapData &data)
    {
        ensureGameParametersInitialized();

        auto filename = getFilename("mining-optimization-training-initialworkers", true);
        if (filename.empty())
        {
            Log::Get() << "ERROR: Could not generate filename for initial workers mining optimization data";
            return;
        }

        zstd::ofstream file(filename, std::ofstream::trunc, COMPRESSION_LEVEL);
        if (file.fail())
        {
            Log::Get() << "Could not open initial workers mining optimization data file for writing: " << filename;
            return;
        }

        bitsery::Serializer<bitsery::OutputStreamAdapter> ser{file};
        serialize<true>(ser, data);
        ser.adapter().flush();
        file.close();

        Log::Get() << "Wrote initial workers mining optimization data to " << filename;
    }
}
