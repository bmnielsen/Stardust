#include "ObservationDataFiles.h"

#include <bitsery/adapter/stream.h>
#include <bitsery/ext/std_set.h>
#include <bitsery/ext/std_map.h>

#include <zstdstream.h>

#include "WorkerMiningOptimization.h"
#include "FileTools.h"

namespace WorkerMiningOptimization
{
    namespace
    {
        GameParameters overriddenGameParameters;

        std::string filename(const std::string &label,
                             bool preferFull,
                             bool postfixWithGatherParameters,
                             bool postfixWithReturnParameters,
                             bool writing)
        {
            auto gameParameters = getGameParameters();

            auto filename = std::ostringstream()
                    << label << "_" << gameParameters.mapHash
                    << "_lf" << gameParameters.latencyFrames;
            if (postfixWithGatherParameters)
            {
                filename << "_" << gameParameters.gatherExploreBefore << "_" << gameParameters.gatherExploreAfter;
            }
            if (postfixWithReturnParameters)
            {
                filename << "_" << gameParameters.returnExploreBefore << "_" << gameParameters.returnExploreAfter;
            }

            if (writing)
            {
                if (preferFull) filename << ".full";
                return FileTools::getFilePath(filename.str(), "bin.zstd", writing);
            }

            auto minimal = FileTools::getFilePath(filename.str(), "bin.zstd", writing);
            filename << ".full";
            auto full = FileTools::getFilePath(filename.str(), "bin.zstd", writing);

            if (!full.empty() && (preferFull || minimal.empty())) return full;
            return minimal;
        }

        std::string optimalGatherPositionsFilename(bool preferFull, bool writing = false)
        {
            return filename("gatherpositions", preferFull, true, false, writing);
        }

        std::string tenDistancePositionsFilename(bool writing = false)
        {
            return filename("10distance", false, false, false, writing);
        }

        std::string optimalReturnPositionsFilename(bool preferFull, bool writing = false)
        {
            return filename("returnpositions", preferFull, false, true, writing);
        }

        struct OptimalGatherPositionsSerializer
        {
            template <typename S>
            void serialize(S& ser,
                           std::map<TilePosition, std::unordered_map<PositionAndVelocity, GatherPositionObservations>> &resourceToOptimalGatherPositions)
            {
                ser.ext(resourceToOptimalGatherPositions,
                        bitsery::ext::StdMap{INT_MAX},
                        [](auto &s, TilePosition &key, std::unordered_map<PositionAndVelocity, GatherPositionObservations> &value)
                        {
                            s.object(key);
                            s.ext(value, bitsery::ext::StdMap{INT_MAX}, [](auto &s, PositionAndVelocity &key, GatherPositionObservations &value)
                            {
                                s.object(key);
                                s.object(value);
                            });
                        });
            }
        };

        struct TenDistancePositionsSerializer
        {
            template <typename S>
            void serialize(S& ser,
                           std::map<TilePosition, std::unordered_set<PositionAndVelocity>> &resourceTo10DistancePositions)
            {
                ser.ext(resourceTo10DistancePositions,
                        bitsery::ext::StdMap{INT_MAX},
                        [](auto &s, TilePosition &key, std::unordered_set<PositionAndVelocity> &value)
                        {
                            s.object(key);
                            s.ext(value, bitsery::ext::StdSet{INT_MAX}, [](auto &s, PositionAndVelocity &value)
                            {
                                s.object(value);
                            });
                        });
            }
        };

        struct OptimalReturnPositionsSerializer
        {
            template <typename S>
            void serialize(S& ser,
                           std::map<TilePosition, std::unordered_map<PositionAndVelocity, ReturnPositionObservations>> &resourceToOptimalReturnPositions)
            {
                ser.ext(resourceToOptimalReturnPositions,
                        bitsery::ext::StdMap{INT_MAX},
                        [](auto &s, TilePosition &key, std::unordered_map<PositionAndVelocity, ReturnPositionObservations> &value)
                        {
                            s.object(key);
                            s.ext(value, bitsery::ext::StdMap{INT_MAX}, [](auto &s, PositionAndVelocity &key, ReturnPositionObservations &value)
                            {
                                s.object(key);
                                s.object(value);
                            });
                        });
            }
        };

        template <typename S, typename M>
        void readDataFile(const std::string &label, const std::string &filename, S serializer, M &data)
        {
            data.clear();

            if (filename.empty())
            {
                Log::Get() << "No saved data available for " << label;
                return;
            }

            zstd::ifstream file(filename);
            if (!file.good())
            {
                Log::Get() << "Could not open saved data file for " << label;
                return;
            }

            bitsery::Deserializer<bitsery::InputStreamAdapter> ser{file};
            serializer.serialize(ser, data);
            file.close();

            Log::Get() << "Read " << label << " data from " << filename;
        }

        template <typename S, typename M>
        void writeDataFile(const std::string &label, const std::string &filename, S serializer, M &data, bool maxCompression)
        {
            zstd::ofstream file(filename, std::ofstream::trunc, maxCompression ? ZSTD_maxCLevel() : 5);
            if (file.fail())
            {
                Log::Get() << "Could not open data file for " << label << " for writing";
                return;
            }

            bitsery::Serializer<bitsery::OutputStreamAdapter> ser{file};
            serializer.serialize(ser, data);
            ser.adapter().flush();
            file.close();

            Log::Get() << "Wrote " << label << " data to " << filename;
        }

        // We don't want to store the position both as part of the data object and the map, so we only store it in the map and copy it after loading
        template <typename T>
        void setPositions(std::map<TilePosition, std::unordered_map<PositionAndVelocity, T>> &map)
        {
            for (auto &[_, patchData] : map)
            {
                for (auto &[pos, metadata] : patchData)
                {
                    metadata.pos = pos;
                }
            }
        }
    }

    void overrideGameParameters(GameParameters gameParameters)
    {
        overriddenGameParameters = std::move(gameParameters);
    }

    GameParameters getGameParameters()
    {
        if (!overriddenGameParameters.mapHash.empty()) return overriddenGameParameters;

        return GameParameters {
            BWAPI::Broodwar->mapHash(),
            BWAPI::Broodwar->getLatencyFrames(),
            GATHER_EXPLORE_BEFORE,
            GATHER_EXPLORE_AFTER,
            RETURN_EXPLORE_BEFORE,
            RETURN_EXPLORE_AFTER
        };
    }

    void readGatherPositionObservations(bool preferFull,
                                        std::map<TilePosition, std::unordered_map<PositionAndVelocity, GatherPositionObservations>> &data)
    {
        readDataFile("gather positions", optimalGatherPositionsFilename(preferFull), OptimalGatherPositionsSerializer{}, data);
        setPositions(data);
    }

    void read10DistanceObservations(std::map<TilePosition, std::unordered_set<PositionAndVelocity>> &data)
    {
        readDataFile("ten-distance positions", tenDistancePositionsFilename(), TenDistancePositionsSerializer{}, data);
    }

    void readReturnPositionObservations(bool preferFull,
                                        std::map<TilePosition, std::unordered_map<PositionAndVelocity, ReturnPositionObservations>> &data)
    {
        readDataFile("return positions", optimalReturnPositionsFilename(preferFull), OptimalReturnPositionsSerializer{}, data);
        setPositions(data);
    }

    void writeGatherPositionObservations(bool minimized,
                                         std::map<TilePosition, std::unordered_map<PositionAndVelocity, GatherPositionObservations>> &data,
                                         bool maxCompresion)
    {
        writeDataFile("gather positions", optimalGatherPositionsFilename(!minimized, true), OptimalGatherPositionsSerializer{}, data, maxCompresion);
    }

    void write10DistanceObservations(std::map<TilePosition, std::unordered_set<PositionAndVelocity>> &data, bool maxCompresion)
    {
        writeDataFile("ten-distance positions", tenDistancePositionsFilename(true), TenDistancePositionsSerializer{}, data, maxCompresion);
    }

    void writeReturnPositionObservations(bool minimized,
                                         std::map<TilePosition, std::unordered_map<PositionAndVelocity, ReturnPositionObservations>> &data,
                                         bool maxCompresion)
    {
        writeDataFile("return positions", optimalReturnPositionsFilename(!minimized, true), OptimalReturnPositionsSerializer{}, data, maxCompresion);
    }
}
