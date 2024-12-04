// Worker mining optimization is split into multiple files
// This file contains the data maps and logic to read and write the data files

#include "WorkerMiningOptimization.h"

#include <bitsery/adapter/stream.h>
#include <bitsery/ext/std_set.h>
#include <bitsery/ext/std_map.h>

#include <zstdstream.h>

#include "TilePosition.h"
#include "PositionAndVelocity.h"
#include "FileTools.h"
#include "CsvTools.h"

#if INSTRUMENTATION_ENABLED
#define OUTPUT_METADATA_ANALYSIS false
#endif

namespace WorkerMiningOptimization
{
    namespace
    {
        // Whether we are exploring new positions
        bool exploring;

        // Metadata for positions used for optimizing approach to the patch
        std::map<TilePosition, std::unordered_map<PositionAndVelocity, GatherPositionObservations>> resourceToOptimalGatherPositions;

        // Metadata for positions LF+1 frames before reaching 10 or less distance from the patch
        // Workers can try to switch to another patch if their chosen patch is being mined once they reach this distance, so we use these
        // positions to detect when we need to start resending gather commands to ensure mineral locking
        std::map<TilePosition, std::unordered_set<PositionAndVelocity>> resourceTo10DistancePositions;

        // Worker state for those on their way to the patch
        std::map<MyWorker, WorkerGatherStatus> workerGatherStatuses;

        // Metadata for positions used for optimizing return of resources
        std::map<TilePosition, std::unordered_map<PositionAndVelocity, ReturnPositionObservations>> resourceToOptimalReturnPositions;

        // Worker state for those returning resources
        std::map<MyWorker, WorkerReturnStatus> workerReturnStatuses;

        std::string optimalGatherPositionsFilename(bool writing = false)
        {
            auto filename = std::ostringstream()
                    << "gatherpositions_" << BWAPI::Broodwar->mapHash()
                    << "_lf" << BWAPI::Broodwar->getLatencyFrames()
                    << "_" << GATHER_EXPLORE_BEFORE << "_" << GATHER_EXPLORE_AFTER;
            return FileTools::getFilePath(filename.str(), "bin.zstd", writing);
        }

        std::string optimalReturnPositionsFilename(bool writing = false)
        {
            auto filename = std::ostringstream()
                    << "returnpositions_" << BWAPI::Broodwar->mapHash()
                    << "_lf" << BWAPI::Broodwar->getLatencyFrames()
                    << "_" << RETURN_EXPLORE_BEFORE << "_" << RETURN_EXPLORE_AFTER;
            return FileTools::getFilePath(filename.str(), "bin.zstd", writing);
        }

        std::string tenDistancePositionsFilename(bool writing = false)
        {
            auto filename = std::ostringstream()
                    << "10distance_" << BWAPI::Broodwar->mapHash()
                    << "_lf" << BWAPI::Broodwar->getLatencyFrames();
            return FileTools::getFilePath(filename.str(), "bin.zstd", writing);
        }

        struct OptimalGatherPositionsSerializer
        {
            template <typename S>
            void serialize(S& ser)
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
            void serialize(S& ser)
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
            void serialize(S& ser)
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

        template <typename S>
        void readDataFile(const std::string &label, const std::string &filename, S serializer)
        {
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
            serializer.serialize(ser);
            file.close();

            Log::Get() << "Read " << label << " data from " << filename;
        }

        template <typename S>
        void writeDataFile(const std::string &label, const std::string &filename, S serializer)
        {
            zstd::ofstream file(filename, std::ofstream::trunc);
            if (file.fail())
            {
                Log::Get() << "Could not open data file for " << label << " for writing";
                return;
            }

            bitsery::Serializer<bitsery::OutputStreamAdapter> ser{file};
            serializer.serialize(ser);
            ser.adapter().flush();
            file.close();

            Log::Get() << "Wrote " << label << " data to " << filename;
        }
    }

    void initialize()
    {
#if INSTRUMENTATION_ENABLED
        exploring = true;
#else
        exploring = false;
#endif
        workerGatherStatuses.clear();
        workerReturnStatuses.clear();

        readDataFile("gather positions", optimalGatherPositionsFilename(), OptimalGatherPositionsSerializer{});
        readDataFile("ten-distance positions", tenDistancePositionsFilename(), TenDistancePositionsSerializer{});
        readDataFile("return positions", optimalReturnPositionsFilename(), OptimalReturnPositionsSerializer{});
    }

    void flushObservations()
    {
        flushGatherObservations(workerGatherStatuses);
        flushReturnObservations(workerReturnStatuses);
    }

    void write()
    {
        writeDataFile("gather positions", optimalGatherPositionsFilename(true), OptimalGatherPositionsSerializer{});
        writeDataFile("ten-distance positions", tenDistancePositionsFilename(true), TenDistancePositionsSerializer{});
        writeDataFile("return positions", optimalReturnPositionsFilename(true), OptimalReturnPositionsSerializer{});

#if OUTPUT_METADATA_ANALYSIS
        {
            int total = 0;
            int unstablePath = 0;
            int unstablePathAtExploreHorizon = 0;
            int neverUsed = 0;
            auto hasBeenUsed = [](const GatherResendArrivalObservations &observations)
            {
                if (observations.empty()) return false;
                if (observations.arrivalDelayAndOccurrences.size() > 1) return true;
                return observations.arrivalDelayAndOccurrences.begin()->second > 1;
            };
            for (const auto &[resource, optimalPositions] : resourceToOptimalGatherPositions)
            {
                for (const auto &[_, optimalPosition] : optimalPositions)
                {
                    total++;
                    bool unstable = false;

                    std::map<int, const SecondResendGatherPositionObservations *> secondResendPositionsByDelta;
                    bool used = hasBeenUsed(optimalPosition.noSecondResendArrivalObservations);
                    for (const auto &[_, secondResendPosition] : optimalPosition.secondResendObservations)
                    {
                        used = used || hasBeenUsed(secondResendPosition.arrivalObservations);
                        unstable = unstable || (secondResendPosition.nextPositionAndOccurrences.size() > 1);
                        if (!unstable)
                        {
                            if (secondResendPositionsByDelta.contains(secondResendPosition.deltaToFirstResend))
                            {
                                unstable = true;
                            }
                            else
                            {
                                secondResendPositionsByDelta[secondResendPosition.deltaToFirstResend] = &secondResendPosition;
                            }
                        }
                    }

                    if (!used) neverUsed++;

                    std::vector<const GatherPositionObservations *> noResendNextPositions = optimalPosition.followingPositionsIfStable(
                            optimalPositions);
                    if (noResendNextPositions.empty() && !optimalPosition.nextPositionAndOccurrences.empty())
                    {
                        unstable = true;
                    }

                    if (unstable)
                    {
                        unstablePath++;
                    }
                    if (unstable && optimalPosition.probableDeltaToBenchmark() == -GATHER_EXPLORE_BEFORE)
                    {
                        unstablePathAtExploreHorizon++;
                    }
                }
            }

            if (total > 0)
            {
                Log::Get() << std::fixed << std::setprecision(1)
                           << "\nStatistics for " << total << " gather resend positions:"
                           << "\nNever used:         " << neverUsed << " (" << (100.0 * (double)neverUsed / (double)total) << "%)"
                           << "\nUnstable path:      " << unstablePath << " (" << (100.0 * (double)unstablePath / (double)total) << "%)"
                           << "\nUnstable path @-" << GATHER_EXPLORE_BEFORE << ": "
                           << unstablePathAtExploreHorizon << " (" << (100.0 * (double)unstablePathAtExploreHorizon / (double)total) << "%)";
            }
        }
#endif
    }

    WorkerGatherStatus &gatherStatusFor(const MyWorker &worker, const MyUnit &depot, const Resource &resource)
    {
        auto workerStatusIt = workerGatherStatuses.find(worker);
        if (workerStatusIt == workerGatherStatuses.end())
        {
            workerStatusIt = workerGatherStatuses.emplace(worker, WorkerGatherStatus{worker, depot, resource}).first;
        }
        else if (workerStatusIt->second.lastProcessedFrame != (currentFrame - 1)
                 || workerStatusIt->second.depot != depot
                 || workerStatusIt->second.resource != resource)
        {
            workerStatusIt->second.reset();
            workerStatusIt->second.depot = depot;
            workerStatusIt->second.resource = resource;
        }
        return workerStatusIt->second;
    }

    WorkerReturnStatus &returnStatusFor(const MyWorker &worker, const MyUnit &depot, const Resource &resource)
    {
        auto workerStatusIt = workerReturnStatuses.find(worker);
        if (workerStatusIt == workerReturnStatuses.end())
        {
            workerStatusIt = workerReturnStatuses.emplace(worker, WorkerReturnStatus{worker, depot, resource}).first;
        }
        else if (workerStatusIt->second.lastProcessedFrame != (currentFrame - 1)
                 || workerStatusIt->second.depot != depot
                 || workerStatusIt->second.resource != resource)
        {
            workerStatusIt->second.reset();
            workerStatusIt->second.depot = depot;
            workerStatusIt->second.resource = resource;
        }
        return workerStatusIt->second;
    }

    WorkerGatherStatus *gatherStatusFor(const MyWorker &worker)
    {
        auto workerStatusIt = workerGatherStatuses.find(worker);
        if (workerStatusIt == workerGatherStatuses.end()) return nullptr;

        return &workerStatusIt->second;
    }

    std::unordered_map<PositionAndVelocity, GatherPositionObservations> &optimalGatherPositionsFor(const Resource &resource)
    {
        return resourceToOptimalGatherPositions[TilePosition::fromBWAPI(resource->tile)];
    }

    std::unordered_map<PositionAndVelocity, ReturnPositionObservations> &optimalReturnPositionsFor(const Resource &resource)
    {
        return resourceToOptimalReturnPositions[TilePosition::fromBWAPI(resource->tile)];
    }

    std::unordered_set<PositionAndVelocity> &tenDistancePositionsFor(const Resource &resource)
    {
        return resourceTo10DistancePositions[TilePosition::fromBWAPI(resource->tile)];
    }

    bool isExploring()
    {
        return exploring;
    }

    void setExploring(bool newExploring)
    {
        exploring = newExploring;
    }
}