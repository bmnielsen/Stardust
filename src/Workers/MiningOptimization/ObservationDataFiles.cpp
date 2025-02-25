#include "ObservationDataFiles.h"

#include <bitsery/adapter/stream.h>
#include <bitsery/traits/vector.h>
#include <bitsery/ext/std_set.h>
#include <bitsery/ext/std_map.h>

#include <zstdstream.h>

#include "WorkerMiningOptimization.h"
#include "FileTools.h"

// Provides the best speed for the use case where we are running explorations (continually saving and reloading)
// Lower levels are not much faster to compress and slower to decompress (because of increased time to read the larger files)
// Higher levels do not compress the files sufficiently better to make up for the increased compression time
#define DEFAULT_COMPRESSION 4

namespace WorkerMiningOptimization::ObservationDataFiles
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

        struct FullOptimalGatherPositionsSerializer
        {
            template <typename S>
            void serialize(S& ser,
                           std::map<TilePosition, std::unordered_map<PositionAndVelocity, GatherPositionObservations>> &resourceToOptimalGatherPositions)
            {
                std::function<void(S&, GatherResendArrivalObservations&)> gatherResendArrivalObservationsSerializer;
                std::function<void(S&, SecondResendGatherPositionObservations&)> secondResendGatherPositionObservationsSerializer;
                std::function<void(S&, GatherPositionObservations&)> gatherPositionObservationsSerializer;

                gatherResendArrivalObservationsSerializer = [](S &s, GatherResendArrivalObservations &value)
                {
                    s.ext(value.arrivalDelayAndOccurrences, bitsery::ext::StdMap{INT_MAX}, [](S& s, int8_t& key, uint32_t& v) {
                        s.value1b(key);
                        s.value4b(v);
                    });
                    s.ext(value.arrivalDelayAndOccurrenceRate, bitsery::ext::StdMap{INT_MAX}, [](S& s, int8_t& key, uint8_t& v) {
                        s.value1b(key);
                        s.value1b(v);
                    });
                    s.value4b(value.collisions);
                    s.value4b(value.nonCollisions);
                    s.value1b(value.collisionRate);
                };

                secondResendGatherPositionObservationsSerializer = [&](S &s, SecondResendGatherPositionObservations &value)
                {
                    s.object(value.pos);
                    s.value4b(value.occurrences);
                    s.value1b(value.occurrenceRate);
                    s.container(value.nextPositions, INT_MAX, [&](S &s, SecondResendGatherPositionObservations &v) {
                        s.object(v, secondResendGatherPositionObservationsSerializer);
                    });
                    s.object(value.arrivalObservations, gatherResendArrivalObservationsSerializer);
                };

                gatherPositionObservationsSerializer = [&](S &s, GatherPositionObservations &value)
                {
                    s.object(value.pos);
                    s.value4b(value.occurrences);
                    s.value1b(value.occurrenceRate);
                    s.ext(value.deltaToBenchmarkAndOccurrences, bitsery::ext::StdMap{INT_MAX}, [](S& s, int8_t& key, uint32_t& v) {
                        s.value1b(key);
                        s.value4b(v);
                    });
                    s.ext(value.deltaToBenchmarkAndOccurrenceRate, bitsery::ext::StdMap{INT_MAX}, [](S& s, int8_t& key, uint8_t& v) {
                        s.value1b(key);
                        s.value1b(v);
                    });
                    s.container(value.nextPositions, INT_MAX, [&](S &s, GatherPositionObservations &v) {
                        s.object(v, gatherPositionObservationsSerializer);
                    });
                    s.object(value.noSecondResendArrivalObservations, gatherResendArrivalObservationsSerializer);
                    s.container(value.secondResendPositions, INT_MAX, [&](S &s, SecondResendGatherPositionObservations &v) {
                        s.object(v, secondResendGatherPositionObservationsSerializer);
                    });
                    s.value4b(value.noResendCollisions);
                    s.value4b(value.noResendNonCollisions);
                    s.value1b(value.noResendCollisionRate);
                };

                ser.ext(resourceToOptimalGatherPositions,
                        bitsery::ext::StdMap{INT_MAX},
                        [&](S &s, TilePosition &key, std::unordered_map<PositionAndVelocity, GatherPositionObservations> &value)
                        {
                            s.object(key);
                            s.ext(value, bitsery::ext::StdMap{INT_MAX}, [&](S &s, PositionAndVelocity &key, GatherPositionObservations &v)
                            {
                                s.object(key);
                                s.object(v, gatherPositionObservationsSerializer);
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

        struct FullOptimalReturnPositionsSerializer
        {
            template <typename S>
            void serialize(S& ser,
                           std::map<TilePosition, std::unordered_map<PositionAndVelocity, ReturnPositionObservations>> &resourceToOptimalReturnPositions)
            {
                std::function<void(S&, ReturnSpeedOccurrences&)> returnSpeedOccurrencesSerializer;
                std::function<void(S&, ReturnArrivalObservations&)> returnArrivalObservationsSerializer;
                std::function<void(S&, ReturnPositionObservations&)> returnPositionObservationsSerializer;

                returnSpeedOccurrencesSerializer = [](S &s, ReturnSpeedOccurrences &value)
                {
                    s.value4b(value.collision);
                    s.value4b(value.lowExitSpeed);
                    s.value4b(value.mediumExitSpeed);
                    s.value4b(value.highExitSpeed);
                };

                returnArrivalObservationsSerializer = [&](S &s, ReturnArrivalObservations &value)
                {
                    s.ext(value.arrivalDelayAndOccurrences, bitsery::ext::StdMap{ INT_MAX }, [](S& s, uint16_t& key, uint32_t& v) {
                        s.value2b(key);
                        s.value4b(v);
                    });
                    s.object(value.deliveryAfterArrivalSpeeds, returnSpeedOccurrencesSerializer);
                    s.object(value.deliveryAtArrivalSpeeds, returnSpeedOccurrencesSerializer);
                };

                returnPositionObservationsSerializer = [&](S &s, ReturnPositionObservations &value)
                {
                    s.object(value.pos);
                    s.value4b(value.occurrences);
                    s.container(value.nextPositions, INT_MAX, [&](S &s, ReturnPositionObservations &v) {
                        s.object(v, returnPositionObservationsSerializer);
                    });
                    s.object(value.noResendArrivalObservations, returnArrivalObservationsSerializer);
                    s.object(value.resendArrivalObservations, returnArrivalObservationsSerializer);
                };

                ser.ext(resourceToOptimalReturnPositions,
                        bitsery::ext::StdMap{INT_MAX},
                        [&](S &s, TilePosition &key, std::unordered_map<PositionAndVelocity, ReturnPositionObservations> &value)
                        {
                            s.object(key);
                            s.ext(value, bitsery::ext::StdMap{INT_MAX}, [&](S &s, PositionAndVelocity &key, ReturnPositionObservations &v)
                            {
                                s.object(key);
                                s.object(v, returnPositionObservationsSerializer);
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
            zstd::ofstream file(filename, std::ofstream::trunc, maxCompression ? ZSTD_maxCLevel() : DEFAULT_COMPRESSION);
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
        readDataFile("gather positions", optimalGatherPositionsFilename(preferFull), FullOptimalGatherPositionsSerializer{}, data);
        setPositions(data);
    }

    void read10DistanceObservations(std::map<TilePosition, std::unordered_set<PositionAndVelocity>> &data)
    {
        readDataFile("ten-distance positions", tenDistancePositionsFilename(), TenDistancePositionsSerializer{}, data);
    }

    void readReturnPositionObservations(bool preferFull,
                                        std::map<TilePosition, std::unordered_map<PositionAndVelocity, ReturnPositionObservations>> &data)
    {
        readDataFile("return positions", optimalReturnPositionsFilename(preferFull), FullOptimalReturnPositionsSerializer{}, data);
        setPositions(data);
    }

    void writeGatherPositionObservations(bool minimized,
                                         std::map<TilePosition, std::unordered_map<PositionAndVelocity, GatherPositionObservations>> &data,
                                         bool maxCompresion)
    {
        writeDataFile("gather positions", optimalGatherPositionsFilename(!minimized, true), FullOptimalGatherPositionsSerializer{}, data, maxCompresion);
    }

    void write10DistanceObservations(std::map<TilePosition, std::unordered_set<PositionAndVelocity>> &data, bool maxCompresion)
    {
        writeDataFile("ten-distance positions", tenDistancePositionsFilename(true), TenDistancePositionsSerializer{}, data, maxCompresion);
    }

    void writeReturnPositionObservations(bool minimized,
                                         std::map<TilePosition, std::unordered_map<PositionAndVelocity, ReturnPositionObservations>> &data,
                                         bool maxCompresion)
    {
        writeDataFile("return positions", optimalReturnPositionsFilename(!minimized, true), FullOptimalReturnPositionsSerializer{}, data, maxCompresion);
    }
}
