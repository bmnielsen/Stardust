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
                    if (line.size() < 10) break;

                    BWAPI::TilePosition tile(std::stoi(line[0]), std::stoi(line[1]));
                    auto resource = Units::resourceAt(tile);
                    if (!resource) continue;

                    if (!PositionAndVelocity::isValidString(line[2]))
                    {
                        Log::Get() << "Invalid position string at line " << lineNumber << "; skipping: " << line[2];
                        continue;
                    }
                    auto pos = PositionAndVelocity::fromString(line[2]);

                    std::shared_ptr<const PositionAndVelocity> next;
                    if (PositionAndVelocity::isValidString(line[3]))
                    {
                        next = std::make_shared<const PositionAndVelocity>(PositionAndVelocity::fromString(line[3]));
                    }

                    auto &resourceMap = map[resource];

                    auto parseObservations = [&](const std::string &str)
                    {
                        std::map<int, int> result;

                        for (const auto &observations : CsvTools::tokenizeList(str, '_'))
                        {
                            auto data = CsvTools::tokenizeList(observations, '|');
                            if (data.size() < 2) continue;

                            result.emplace(std::stoi(data[0]), std::stoi(data[1]));
                        }

                        return ResendPositionObservations{result};
                    };

                    auto parseSecondResendPositions = [&]()
                    {
                        std::vector<SecondResendPositionObservationMetadata> result;
                        if (line.size() < 11) return result;

                        for (const auto &secondResendData : CsvTools::tokenizeList(line[10]))
                        {
                            auto data = CsvTools::tokenizeList(secondResendData, ':');
                            if (data.size() < 2) continue;
                            if (!PositionAndVelocity::isValidString(data[0])) continue;

                            result.emplace_back(SecondResendPositionObservationMetadata{
                                    PositionAndVelocity::fromString(data[0]),
                                    std::stoi(data[1]),
                                    (data.size() > 2) ? parseObservations(data[2]) : ResendPositionObservations{std::map<int, int>{}}
                            });
                        }

                        return result;
                    };

                    resourceMap.emplace(pos, PositionObservationMetadata{
                        pos,
                        next,
                        std::stoi(line[4]),
                        std::stoi(line[5]),
                        std::stoi(line[6]),
                        line[7] == "y",
                        line[8] == "y",
                        parseObservations(line[9]),
                        parseSecondResendPositions()
                    });

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

            auto outputObservations = [&file](const ResendPositionObservations &observations)
            {
                std::string sep;
                for (const auto &[delta, occurrences] : observations.data)
                {
                    file << sep << delta << "|" << occurrences;
                    sep = "_";
                }
            };

            int count = 0;
            for (const auto &[resource, gatherPositions] : map)
            {
                for (const auto &[resendPos, resendPositionMetadata] : gatherPositions)
                {
                    file << resource->tile.x << ";"
                         << resource->tile.y << ";"
                         << resendPos << ";";
                    if (resendPositionMetadata.next) file << *resendPositionMetadata.next;
                    file << ";"
                         << resendPositionMetadata.deltaToNormalPathOptimalPosition << ";"
                         << resendPositionMetadata.bestDelta << ";"
                         << resendPositionMetadata.bestFollowingPositionDelta << ";"
                         << (resendPositionMetadata.hasPositionToTry ? "y" : "") << ";"
                         << (resendPositionMetadata.followingHasPositionToTry ? "y" : "") << ";";
                    outputObservations(resendPositionMetadata.noResendObservations);
                    file << ";";

                    std::string sep;
                    for (const auto &secondResendPositionMetadata : resendPositionMetadata.secondResendMetadata)
                    {
                        file << sep
                             << secondResendPositionMetadata.pos << ":"
                             << secondResendPositionMetadata.deltaToFirstResend << ":";
                        outputObservations(secondResendPositionMetadata.observations);
                        sep = ",";
                    }

                    // TODO: Remove following debugging things once everything is working
                    file << ";";
                    if (resendPositionMetadata.bestDelta < resendPositionMetadata.bestFollowingPositionDelta &&
                        resendPositionMetadata.deltaToNormalPathOptimalPosition < 0)
                    {
                        file << "*" << resendPositionMetadata.bestDelta;
                    }

                    int mostOccurrences = 0;
                    int mostOccurrencesPosition = -1;
                    auto handleObservations = [&](const ResendPositionObservations &observations, int thisPosition)
                    {
                        for (const auto &[delta, occurrences] : observations.data)
                        {
                            if (occurrences >= mostOccurrences)
                            {
                                mostOccurrences = occurrences;
                                mostOccurrencesPosition = thisPosition;
                            }
                        }

#if INSTRUMENTATION_ENABLED
                        if (observations.data.size() > 1)
                        {
                            int mostCommonArrivalDelay = observations.mostCommonArrivalDelay();
                            int common = 0;
                            int uncommon = 0;
                            int problematic = 0;
                            for (const auto &[delta, occurrences] : observations.data)
                            {
                                ((delta == mostCommonArrivalDelay) ? common : uncommon) += occurrences;
                                if (delta >= 0 && delta < mostCommonArrivalDelay) problematic++;
                            }

                            if ((common + uncommon) > 10 && (common < uncommon * 4))
                            {
                                Log::Get() << "WARNING: Patch " << resource->tile
                                    << " position " << resendPositionMetadata << " : " << thisPosition
                                    << " has " << observations.data.size() << " unstable arrival deltas"
                                    << "; common(" << mostCommonArrivalDelay << ")=" << common
                                    << "; uncommon=" << uncommon
                                    << "; problematic=" << problematic;
                            }
                        }
#endif
                    };
                    handleObservations(resendPositionMetadata.noResendObservations, 0);
                    for (const auto &secondResendMetadata : resendPositionMetadata.secondResendMetadata)
                    {
                        handleObservations(secondResendMetadata.observations, secondResendMetadata.deltaToFirstResend);
                    }
                    if (mostOccurrences > 1)
                    {
                        file << ";y;" << mostOccurrencesPosition;
                    }
                    else
                    {
                        file << ";;";
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