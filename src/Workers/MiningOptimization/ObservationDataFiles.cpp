#include "ObservationDataFiles.h"

#include <bitsery/adapter/stream.h>
#include <bitsery/traits/vector.h>
#include <bitsery/ext/std_set.h>
#include <bitsery/ext/std_map.h>

#include <zstdstream.h>

#include <filesystem>

#include "WorkerMiningOptimization.h"
#include "FileTools.h"

// Provides the best speed for the use case where we are running explorations (continually saving and reloading)
// Lower levels are not much faster to compress and slower to decompress (because of increased time to read the larger files)
// Higher levels do not compress the files sufficiently better to make up for the increased compression time
#define DEFAULT_COMPRESSION 4

#define EXPORT_PATH "/Users/bmnielsen/BW/mining-data/dist/"

namespace WorkerMiningOptimization::ObservationDataFiles
{
    namespace
    {
        GameParameters overriddenGameParameters;

        std::pair<std::string, bool> filename(const std::string &label,
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
                return std::make_pair(FileTools::getFilePath(filename.str(), "bin.zstd", writing), preferFull);
            }

            auto minimal = FileTools::getFilePath(filename.str(), "bin.zstd", writing);
            filename << ".full";
            auto full = FileTools::getFilePath(filename.str(), "bin.zstd", writing);

            if (!full.empty() && (preferFull || minimal.empty())) return std::make_pair(full, true);
            return std::make_pair(minimal, false);
        }

        std::string exportFilename(const std::string &label, bool postfixWithGatherParameters, bool postfixWithReturnParameters)
        {
            auto gameParameters = getGameParameters();

            auto filename = std::ostringstream()
                    << label << "_" << gameParameters.exportMapHash
                    << "_lf" << gameParameters.latencyFrames;
            if (postfixWithGatherParameters)
            {
                filename << "_" << gameParameters.gatherExploreBefore << "_" << gameParameters.gatherExploreAfter;
            }
            if (postfixWithReturnParameters)
            {
                filename << "_" << gameParameters.returnExploreBefore << "_" << gameParameters.returnExploreAfter;
            }

            return (std::ostringstream() << EXPORT_PATH << filename.str() << ".bin.zstd").str();
        }

        std::pair<std::string, bool> optimalGatherPositionsFilename(bool preferFull, bool writing = false)
        {
            return filename("gatherpositions", preferFull, true, false, writing);
        }

        std::string optimalGatherPositionsExportFilename()
        {
            return exportFilename("gatherpositions", true, false);
        }

        std::string tenDistancePositionsFilename(bool writing = false)
        {
            return filename("10distance", false, false, false, writing).first;
        }

        std::string tenDistancePositionsExportFilename()
        {
            return exportFilename("10distance", false, false);
        }

        std::pair<std::string, bool> optimalReturnPositionsFilename(bool preferFull, bool writing = false)
        {
            return filename("returnpositions", preferFull, false, true, writing);
        }

        std::string optimalReturnPositionsExportFilename()
        {
            return exportFilename("returnpositions", false, true);
        }

        std::pair<std::string, bool> resourceObservationsFilename(bool preferFull, bool writing = false)
        {
            return filename("resources", preferFull, false, true, writing);
        }

        std::string resourceObservationsExportFilename()
        {
            return exportFilename("resources", false, true);
        }

        template<bool full>
        struct OptimalGatherPositionsSerializer
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
                    if constexpr (full)
                    {
                        s.ext(value.arrivalDelayAndOccurrences, bitsery::ext::StdMap{INT_MAX}, [](S& s, int8_t& key, uint32_t& v) {
                            s.value1b(key);
                            s.value4b(v);
                        });
                    }
                    s.ext(value.arrivalDelayAndOccurrenceRate, bitsery::ext::StdMap{INT_MAX}, [](S& s, int8_t& key, uint8_t& v) {
                        s.value1b(key);
                        s.value1b(v);
                    });
                    if constexpr (full)
                    {
                        s.value4b(value.collisions);
                        s.value4b(value.nonCollisions);
                    }
                    s.value1b(value.collisionRate);
                };

                secondResendGatherPositionObservationsSerializer = [&](S &s, SecondResendGatherPositionObservations &value)
                {
                    s.object(value.pos);
                    if constexpr (full)
                    {
                        s.value4b(value.occurrences);
                    }
                    s.value1b(value.occurrenceRate);
                    s.container(value.nextPositions, INT_MAX, [&](S &s, SecondResendGatherPositionObservations &v) {
                        s.object(v, secondResendGatherPositionObservationsSerializer);
                    });
                    s.object(value.arrivalObservations, gatherResendArrivalObservationsSerializer);
                };

                gatherPositionObservationsSerializer = [&](S &s, GatherPositionObservations &value)
                {
                    s.object(value.pos);
                    if constexpr (full)
                    {
                        s.value4b(value.occurrences);
                    }
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
                    if constexpr (full)
                    {
                        s.value4b(value.noResendCollisions);
                        s.value4b(value.noResendNonCollisions);
                    }
                    s.value1b(value.noResendCollisionRate);
                };

                ser.ext(resourceToOptimalGatherPositions,
                        bitsery::ext::StdMap{INT_MAX},
                        [&](S &s, TilePosition &key, std::unordered_map<PositionAndVelocity, GatherPositionObservations> &value)
                        {
                            s.object(key);
                            s.ext(value, bitsery::ext::StdMap{INT_MAX}, [&](S &s, PositionAndVelocity &key, GatherPositionObservations &v)
                            {
                                s.object(v, gatherPositionObservationsSerializer);
                                key = v.pos;
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

        template<bool full>
        struct OptimalReturnPositionsSerializer
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
                    if constexpr (full)
                    {
                        s.value4b(value.collision);
                        s.value4b(value.lowExitSpeed);
                        s.value4b(value.mediumExitSpeed);
                        s.value4b(value.highExitSpeed);
                    }
                    s.value1b(value.frameDelay);
                };

                returnArrivalObservationsSerializer = [&](S &s, ReturnArrivalObservations &value)
                {
                    if constexpr (full)
                    {
                        s.ext(value.arrivalDelayAndOccurrences, bitsery::ext::StdMap{ INT_MAX }, [](S& s, uint8_t& key, uint32_t& v) {
                            s.value1b(key);
                            s.value4b(v);
                        });
                    }
                    s.ext(value.arrivalDelayAndOccurrenceRate, bitsery::ext::StdMap{ INT_MAX }, [](S& s, uint8_t& key, uint8_t& v) {
                        s.value1b(key);
                        s.value1b(v);
                    });
                    s.object(value.deliveryAfterArrivalSpeeds, returnSpeedOccurrencesSerializer);
                    s.object(value.deliveryAtArrivalSpeeds, returnSpeedOccurrencesSerializer);
                };

                returnPositionObservationsSerializer = [&](S &s, ReturnPositionObservations &value)
                {
                    s.object(value.pos);
                    if constexpr (full)
                    {
                        s.value4b(value.occurrences);
                    }
                    s.value1b(value.occurrenceRate);
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
                                s.object(v, returnPositionObservationsSerializer);
                                key = v.pos;
                            });
                        });
            }
        };

        template<bool full>
        struct ResourceObservationsSerializer
        {
            template <typename S>
            void serialize(S& ser,
                           std::map<TilePosition, ResourceObservations> &resourceToResourceObservations)
            {
                auto resourceObservationSerializer = [](S &s, ResourceObservation &value)
                {
                    if constexpr (full)
                    {
                        s.value4b(value.observationCount);
                        s.value8b(value.accumulator);
                        s.value8b(value.varianceAccumulator);
                    }
                    s.value2b(value.average);
                    s.value2b(value.variance);
                };

                auto resourceObservationsSerializer = [&](S &s, ResourceObservations &value)
                {
                    s.object(value.singleWorkerRotations, resourceObservationSerializer);
                    s.object(value.doubleWorkerRotations, resourceObservationSerializer);
                    s.container(value.startingWorkerObservations, 4, [&](S &s, ResourceObservation &v) {
                        s.object(v, resourceObservationSerializer);
                    });
                };

                ser.ext(resourceToResourceObservations,
                        bitsery::ext::StdMap{INT_MAX},
                        [&](S &s, TilePosition &key, ResourceObservations &value)
                        {
                            s.object(key);
                            s.object(value, resourceObservationsSerializer);
                            value.tile = key;
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

        void copyDataFile(const std::string &source, const std::string &target)
        {
            std::filesystem::copy(source, target, std::filesystem::copy_options::overwrite_existing);
            Log::Get() << "Copied " << source << " to " << target;
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
            "",
            BWAPI::Broodwar->getLatencyFrames(),
            GATHER_EXPLORE_BEFORE,
            GATHER_EXPLORE_AFTER,
            RETURN_EXPLORE_BEFORE,
            RETURN_EXPLORE_AFTER
        };
    }

    void readGatherPositionObservations(bool requireFull,
                                        std::map<TilePosition, std::unordered_map<PositionAndVelocity, GatherPositionObservations>> &data)
    {
        auto [filename, full] = optimalGatherPositionsFilename(requireFull);

        if (requireFull && !full)
        {
            data.clear();
            return;
        }

        if (full)
        {
            readDataFile("gather positions", filename, OptimalGatherPositionsSerializer<true>{}, data);
        }
        else
        {
            readDataFile("gather positions", filename, OptimalGatherPositionsSerializer<false>{}, data);
        }
    }

    void read10DistanceObservations(std::map<TilePosition, std::unordered_set<PositionAndVelocity>> &data)
    {
        readDataFile("ten-distance positions", tenDistancePositionsFilename(), TenDistancePositionsSerializer{}, data);
    }

    void readReturnPositionObservations(bool requireFull,
                                        std::map<TilePosition, std::unordered_map<PositionAndVelocity, ReturnPositionObservations>> &data)
    {
        auto [filename, full] = optimalReturnPositionsFilename(requireFull);

        if (requireFull && !full)
        {
            data.clear();
            return;
        }

        if (full)
        {
            readDataFile("return positions", filename, OptimalReturnPositionsSerializer<true>{}, data);
        }
        else
        {
            readDataFile("return positions", filename, OptimalReturnPositionsSerializer<false>{}, data);
        }
    }

    void readResourceObservations(bool requireFull,
                                  std::map<TilePosition, ResourceObservations> &data)
    {
        auto [filename, full] = resourceObservationsFilename(requireFull);

        if (requireFull && !full)
        {
            data.clear();
            return;
        }

        if (full)
        {
            readDataFile("resource observations", filename, ResourceObservationsSerializer<true>{}, data);
        }
        else
        {
            readDataFile("resource observations", filename, ResourceObservationsSerializer<false>{}, data);
        }
    }

    void writeGatherPositionObservations(bool minimized,
                                         std::map<TilePosition, std::unordered_map<PositionAndVelocity, GatherPositionObservations>> &data,
                                         bool maxCompression)
    {
        if (minimized)
        {
            auto filename = optimalGatherPositionsFilename(false, true).first;
            writeDataFile("gather positions",
                          filename,
                          OptimalGatherPositionsSerializer<false>{},
                          data,
                          maxCompression);
            if (!getGameParameters().exportMapHash.empty())
            {
                copyDataFile(filename, optimalGatherPositionsExportFilename());
            }
        }
        else
        {
            writeDataFile("gather positions",
                          optimalGatherPositionsFilename(true, true).first,
                          OptimalGatherPositionsSerializer<true>{},
                          data,
                          maxCompression);
        }
    }

    void write10DistanceObservations(std::map<TilePosition, std::unordered_set<PositionAndVelocity>> &data, bool maxCompression)
    {
        auto filename = tenDistancePositionsFilename(true);
        writeDataFile("ten-distance positions", filename, TenDistancePositionsSerializer{}, data, maxCompression);
        if (!getGameParameters().exportMapHash.empty())
        {
            copyDataFile(filename, tenDistancePositionsExportFilename());
        }
    }

    void writeReturnPositionObservations(bool minimized,
                                         std::map<TilePosition, std::unordered_map<PositionAndVelocity, ReturnPositionObservations>> &data,
                                         bool maxCompression)
    {
        if (minimized)
        {
            auto filename = optimalReturnPositionsFilename(false, true).first;
            writeDataFile("return positions",
                          filename,
                          OptimalReturnPositionsSerializer<false>{},
                          data,
                          maxCompression);
            if (!getGameParameters().exportMapHash.empty())
            {
                copyDataFile(filename, optimalReturnPositionsExportFilename());
            }
        }
        else
        {
            writeDataFile("return positions",
                          optimalReturnPositionsFilename(true, true).first,
                          OptimalReturnPositionsSerializer<true>{},
                          data,
                          maxCompression);
        }
    }

    void writeResourceObservations(bool minimized,
                                   std::map<TilePosition, ResourceObservations> &data,
                                   bool maxCompression)
    {
        if (minimized)
        {
            auto filename = resourceObservationsFilename(false, true).first;

            writeDataFile("resource observations",
                          filename,
                          ResourceObservationsSerializer<false>{},
                          data,
                          maxCompression);
            if (!getGameParameters().exportMapHash.empty())
            {
                copyDataFile(filename, resourceObservationsExportFilename());
            }
        }
        else
        {
            writeDataFile("resource observations",
                          resourceObservationsFilename(true, true).first,
                          ResourceObservationsSerializer<true>{},
                          data,
                          maxCompression);
        }
    }
}
