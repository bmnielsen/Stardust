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
                    << "gatherpositions_" << BWAPI::Broodwar->mapHash()
                    << "_lf" << BWAPI::Broodwar->getLatencyFrames();
            return FileTools::getFilePath(filename.str(), "csv", writing);
        }

        std::string tenDistancePositionsFilename(bool writing = false)
        {
            auto filename = std::ostringstream()
                    << "10distance_" << BWAPI::Broodwar->mapHash()
                    << "_lf" << BWAPI::Broodwar->getLatencyFrames();
            return FileTools::getFilePath(filename.str(), "csv", writing);
        }

        std::string takeoverResendPositionsFilename(bool writing = false)
        {
            auto filename = std::ostringstream()
                    << "takeoverpositions_" << BWAPI::Broodwar->mapHash()
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
                    if (line.size() < 6) break;

                    BWAPI::TilePosition tile(std::stoi(line[0]), std::stoi(line[1]));
                    auto resource = Units::resourceAt(tile);
                    if (!resource) continue;

                    if (!PositionAndVelocity::isValidString(line[2]))
                    {
                        Log::Get() << "Invalid position string at line " << lineNumber << "; skipping: " << line[2];
                        continue;
                    }
                    auto pos = PositionAndVelocity::fromString(line[2]);

                    std::map<PositionAndVelocity, std::map<int, PositionObservationMetadata>> failurePositionMetadata;
                    if (line.size() > 6)
                    {
                        for (const auto &nonoptimalResendOptimalPosition : CsvTools::tokenizeList(line[6]))
                        {
                            auto data = CsvTools::tokenizeList(nonoptimalResendOptimalPosition, ':');
                            if (data.size() < 5) continue;
                            if (!PositionAndVelocity::isValidString(data[0])) continue;
                            auto nonoptimalPos = PositionAndVelocity::fromString(data[0]);

                            failurePositionMetadata[nonoptimalPos].emplace(std::stoi(data[1]), PositionObservationMetadata{
                                    nonoptimalPos,
                                    (unsigned int)std::stoi(data[2]),
                                    (unsigned int)std::stoi(data[3]),
                                    (unsigned int)std::stoi(data[4])});
                        }
                    }

                    map[resource].emplace(
                            pos,
                            PositionObservationMetadata{
                                    pos,
                                    (unsigned int)std::stoi(line[3]),
                                    (unsigned int)std::stoi(line[4]),
                                    (unsigned int)std::stoi(line[5]),
                                    failurePositionMetadata});
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
                    file << resourceAndOptimalGatherPositions.first->tile.x << ";"
                         << resourceAndOptimalGatherPositions.first->tile.y << ";"
                         << optimalOrderPosition.first << ";"
                         << optimalOrderPosition.second.observations << ";"
                         << optimalOrderPosition.second.successes << ";"
                         << optimalOrderPosition.second.failures << ";";
                    std::string sep;
                    for (const auto &[pos, deltaAndMetadata] : optimalOrderPosition.second.failurePositionMetadata)
                    {
                        for (const auto &[delta, metadata] : deltaAndMetadata)
                        {
                            file << sep << pos << ":" << delta << ":"
                                 << metadata.observations << ":"
                                 << metadata.successes << ":"
                                 << metadata.failures;
                            sep = ",";
                        }
                    }
                    file << "\n";
                    count++;
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