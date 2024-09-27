// Worker mining optimization is split into multiple files
// This file contains the data maps and logic to read and write the data files

#include "WorkerMiningOptimization.h"

#include "PositionAndVelocity.h"
#include "FileTools.h"
#include "CsvTools.h"
#include "Units.h"

namespace WorkerMiningOptimization
{
    namespace
    {
        // Metadata for positions where we can resend the gather command to start mining immediately on arrival
        std::map<Resource, std::map<PositionAndVelocity, PositionObservationMetadata>> resourceToOptimalGatherPositions;

        // Metadata for positions LF+1 frames before reaching 10 or less distance from the patch
        // Workers can try to switch to another patch if their chosen patch is being mined once they reach this distance, so we use these
        // positions to detect when we need to start resending gather commands to ensure mineral locking
        std::map<Resource, std::map<PositionAndVelocity, PositionObservationMetadata>> resourceTo10DistancePositions;

        // Metadata for positions where we can resend the gather command to optimize the takeover frame and reach the patch on time
        std::map<Resource, std::map<PositionAndVelocity, PositionObservationMetadata>> resourceToTakeoverResendPositions;

        // Worker state for those on their way to the patch
        std::map<MyWorker, WorkerGatherStatus> workerGatherStatuses;

        std::string optimalGatherPositionsFilename(bool writing = false)
        {
            auto filename = std::ostringstream()
                    << "gatherpositionsnew_" << BWAPI::Broodwar->mapHash()
                    << "_lf" << BWAPI::Broodwar->getLatencyFrames();
            return FileTools::getFilePath(filename.str(), "csv", writing);
        }

        std::string tenDistancePositionsFilename(bool writing = false)
        {
            auto filename = std::ostringstream()
                    << "10distancenew_" << BWAPI::Broodwar->mapHash()
                    << "_lf" << BWAPI::Broodwar->getLatencyFrames();
            return FileTools::getFilePath(filename.str(), "csv", writing);
        }

        std::string takeoverResendPositionsFilename(bool writing = false)
        {
            auto filename = std::ostringstream()
                    << "takeoverpositionsnew_" << BWAPI::Broodwar->mapHash()
                    << "_lf" << BWAPI::Broodwar->getLatencyFrames();
            return FileTools::getFilePath(filename.str(), "csv", writing);
        }

        void parsePositionObservationMetadataFile(const std::string &filename,
                                                  std::map<Resource, std::map<PositionAndVelocity, PositionObservationMetadata>> &map)
        {
            map.clear();

            std::ifstream file;
            file.open(filename);
            if (!file.good()) return;

            int lineNumber = 0;
            try
            {
                // Read and parse each position
                int count = 0;
                while (true)
                {
                    lineNumber++;

                    auto line = CsvTools::readNextLine(file);
                    if (line.size() < 8) break;

                    BWAPI::TilePosition tile(std::stoi(line[0]), std::stoi(line[1]));
                    auto resource = Units::resourceAt(tile);
                    if (!resource) continue;

                    if (!PositionAndVelocity::isValidString(line[2]))
                    {
                        Log::Get() << "Invalid position string at line " << lineNumber << "; skipping: " << line[2];
                        continue;
                    }
                    auto pos = PositionAndVelocity::fromString(line[2]);

                    auto &resourceMap = map[resource];
                    auto positionDataIt = resourceMap.find(pos);
                    if (positionDataIt == resourceMap.end())
                    {
                        positionDataIt = resourceMap.emplace(pos, PositionObservationMetadata{
                            pos,
                            std::stoi(line[3]),
                            std::stoi(line[4]),
                            std::stoi(line[5]),
                            line[6] == "y"
                        }).first;
                    }
                    auto &positionData = positionDataIt->second;

                    auto addObservationsTo = [&](std::map<int, int> &observationsMap)
                    {
                        if (line.size() < 9) return;

                        for (const auto &observations : CsvTools::tokenizeList(line[8]))
                        {
                            auto data = CsvTools::tokenizeList(observations, ':');
                            if (data.size() < 2) continue;

                            observationsMap.emplace(std::stoi(data[0]), std::stoi(data[1]));
                        }
                    };

                    if (PositionAndVelocity::isValidString(line[7]))
                    {
                        auto secondResendPos = PositionAndVelocity::fromString(line[7]);
                        addObservationsTo(positionData.resendPositionToData[secondResendPos]);
                    }
                    else
                    {
                        addObservationsTo(positionData.noResendData);
                    }

                    count++;
                }

                Log::Get() << "Read " << count << " positions metadata from " << filename;
            }
            catch (std::exception &ex)
            {
                Log::Get() << "Exception caught attempting to read positions metadata from " << filename
                           << " at line " << lineNumber << ": " << ex.what();
            }
        }

        void writePositionObservationMetadataFile(const std::string &filename,
                                                  std::map<Resource, std::map<PositionAndVelocity, PositionObservationMetadata>> &map)
        {
            std::ofstream file;
            file.open(filename, std::ofstream::trunc);

            int count = 0;
            for (auto &resourceAndOptimalGatherPositions : map)
            {
                for (auto &optimalOrderPosition : resourceAndOptimalGatherPositions.second)
                {
                    auto out = [&](const PositionAndVelocity *secondResendPosition, const std::map<int, int> &observations)
                    {
                        file << resourceAndOptimalGatherPositions.first->tile.x << ";"
                             << resourceAndOptimalGatherPositions.first->tile.y << ";"
                             << optimalOrderPosition.first << ";"
                             << optimalOrderPosition.second.deltaToNormalPathOptimalPosition << ";"
                             << optimalOrderPosition.second.bestDelta << ";"
                             << optimalOrderPosition.second.bestFollowingPositionDelta << ";";
                        if (optimalOrderPosition.second.followingHasUntriedPosition)
                        {
                            file << "y";
                        }
                        file << ";";

                        if (secondResendPosition) file << *secondResendPosition;
                        file << ";";

                        std::string sep;
                        for (const auto &[delta, occurrences] : observations)
                        {
                            file << sep << delta << ":" << occurrences;
                            sep = ",";
                        }

                        // TODO: remove later, just for analysis purposes
                        file << ";";
                        if (optimalOrderPosition.second.bestDelta == 0 && optimalOrderPosition.second.deltaToNormalPathOptimalPosition < 0)
                        {
                            file << "*" << -optimalOrderPosition.second.deltaToNormalPathOptimalPosition << "*";
                        }

                        file << "\n";
                        count++;
                    };

                    out(nullptr, optimalOrderPosition.second.noResendData);

                    for (auto &[secondResendPosition, observations] : optimalOrderPosition.second.resendPositionToData)
                    {
                        out(&secondResendPosition, observations);
                    }
                }
            }

            file.close();
            Log::Get() << "Wrote " << count << " positions metadata to " << filename;
        }
    }

    void initialize()
    {
        workerGatherStatuses.clear();

        parsePositionObservationMetadataFile(optimalGatherPositionsFilename(), resourceToOptimalGatherPositions);
        parsePositionObservationMetadataFile(tenDistancePositionsFilename(), resourceTo10DistancePositions);
        parsePositionObservationMetadataFile(takeoverResendPositionsFilename(), resourceToTakeoverResendPositions);
    }

    void flushObservations()
    {
        flushStartOfMiningObservations(workerGatherStatuses);

        // TODO: Add return when ready
    }

    void write()
    {
        writePositionObservationMetadataFile(optimalGatherPositionsFilename(true), resourceToOptimalGatherPositions);
        writePositionObservationMetadataFile(tenDistancePositionsFilename(true), resourceTo10DistancePositions);
        writePositionObservationMetadataFile(takeoverResendPositionsFilename(true), resourceToTakeoverResendPositions);
    }

    // Optimizes returning a resource
    void optimizeReturnOfResource(const MyWorker &worker, const MyUnit &depot, const Resource &resource)
    {
        // TODO
    }

    WorkerGatherStatus &gatherStatusFor(const MyWorker &worker, const Resource &resource)
    {
        auto workerStatusIt = workerGatherStatuses.find(worker);
        if (workerStatusIt == workerGatherStatuses.end())
        {
            workerStatusIt = workerGatherStatuses.emplace(worker, WorkerGatherStatus{worker, resource}).first;
        }
        return workerStatusIt->second;
    }

    std::map<PositionAndVelocity, PositionObservationMetadata> &optimalGatherPositionsFor(const Resource &resource)
    {
        return resourceToOptimalGatherPositions[resource];
    }

    std::map<PositionAndVelocity, PositionObservationMetadata> &tenDistancePositionsFor(const Resource &resource)
    {
        return resourceTo10DistancePositions[resource];
    }

    std::map<PositionAndVelocity, PositionObservationMetadata> &takeoverPositionsFor(const Resource &resource)
    {
        return resourceToTakeoverResendPositions[resource];
    }
}