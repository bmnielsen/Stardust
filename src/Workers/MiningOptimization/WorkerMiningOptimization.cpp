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
        // Whether we are exploring new positions
        bool exploring;

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
                    << "_lf" << BWAPI::Broodwar->getLatencyFrames()
                    << "_" << EXPLORE_BEFORE << EXPLORE_AFTER << EXPLORE_SECOND_RESEND_POSITIONS;
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

            auto parseNextPositions = [](const std::string &str)
            {
                std::map<PositionAndVelocity, int> result;

                for (const auto &observations : CsvTools::tokenizeList(str, '_'))
                {
                    auto data = CsvTools::tokenizeList(observations, '|');
                    if (data.size() < 2) continue;
                    if (!PositionAndVelocity::isValidString(data[0])) continue;

                    result.emplace(PositionAndVelocity::fromString(data[0]), std::stoi(data[1]));
                }

                return result;
            };

            auto parseObservations = [](const std::string &str)
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

            auto parseSecondResendPositions = [&parseNextPositions, &parseObservations](const std::string &str)
            {
                std::map<PositionAndVelocity, SecondResendPositionObservationMetadata> result;
                if (str.empty()) return result;

                for (const auto &secondResendData : CsvTools::tokenizeList(str))
                {
                    auto data = CsvTools::tokenizeList(secondResendData, ':');
                    if (data.size() < 3) continue;
                    if (!PositionAndVelocity::isValidString(data[0])) continue;

                    auto pos = PositionAndVelocity::fromString(data[0]);

                    result.emplace(pos, SecondResendPositionObservationMetadata{
                            pos,
                            parseNextPositions(data[1]),
                            std::stoi(data[2]),
                            (data.size() > 3) ? parseObservations(data[3]) : ResendPositionObservations{std::map<int, int>{}}
                    });
                }

                return result;
            };

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
                    if (line.size() < 7) break;

                    BWAPI::TilePosition tile(std::stoi(line[0]), std::stoi(line[1]));
                    auto resource = Units::resourceAt(tile);
                    if (!resource) continue;

                    if (!PositionAndVelocity::isValidString(line[3]))
                    {
                        Log::Get() << "Invalid position string at line " << lineNumber << "; skipping: " << line[2];
                        continue;
                    }
                    auto pos = PositionAndVelocity::fromString(line[3]);

                    auto &resourceMap = map[resource];

                    resourceMap.emplace(pos, PositionObservationMetadata{
                        (uint32_t)std::stoul(line[2]),
                        pos,
                        parseNextPositions(line[4]),
                        std::stoi(line[5]),
                        parseObservations(line[6]),
                        parseSecondResendPositions((line.size() > 7) ? line[7] : "")
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

            auto outputNext = [&file](const std::map<PositionAndVelocity, int> &next)
            {
                std::string nextPosSep;
                for (const auto &[nextPos, nextOccurrences] : next)
                {
                    file << nextPosSep << nextPos << "|" << nextOccurrences;
                    nextPosSep = "_";
                }
            };
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
                         << resendPositionMetadata.pathHash << ";"
                         << resendPos << ";";

                    outputNext(resendPositionMetadata.next);
                    file << ";";

                    file << resendPositionMetadata.deltaToNormalPathOptimalPosition << ";";

                    outputObservations(resendPositionMetadata.noResendObservations);
                    file << ";";

                    std::string secondResendPosSep;
                    for (const auto &[secondResentPos, secondResendPositionMetadata] : resendPositionMetadata.secondResendMetadata)
                    {
                        file << secondResendPosSep
                             << secondResendPositionMetadata.pos << ":";
                        outputNext(secondResendPositionMetadata.next);
                        file << ":"
                             << secondResendPositionMetadata.deltaToFirstResend << ":";
                        outputObservations(secondResendPositionMetadata.observations);
                        secondResendPosSep = ",";
                    }
                    file << ";";

                    // TODO: Remove following debugging things once everything is working

                    // Most common delta
                    int bestDelta = 100;
                    auto handleObservations = [&resendPositionMetadata, &bestDelta](const ResendPositionObservations &observations, int addedDelta)
                    {
                        if (observations.empty()) return;

                        int arrivalDelay = observations.mostCommonArrivalDelay();
                        if (arrivalDelay < 0) return;

                        int delta = resendPositionMetadata.deltaToNormalPathOptimalPosition + addedDelta + arrivalDelay;
                        if (delta < bestDelta) bestDelta = delta;
                    };
                    handleObservations(resendPositionMetadata.noResendObservations, 0);
                    for (const auto &secondResendPosition : resendPositionMetadata.expectedPathAfterResend())
                    {
                        handleObservations(secondResendPosition->observations, secondResendPosition->deltaToFirstResend);
                    }
                    file << bestDelta << ";";

                    // Number of next positions this position has
                    file << resendPositionMetadata.next.size() << ";";

                    // Number of second resend paths this position has
                    std::map<int, int> secondResendDeltaOccurrences;
                    for (const auto &[_, secondResendPosition] : resendPositionMetadata.secondResendMetadata)
                    {
                        secondResendDeltaOccurrences[secondResendPosition.deltaToFirstResend]++;
                    }
                    int maxSecondResendDeltaOccurrences = 0;
                    for (const auto &[_, occurrences] : secondResendDeltaOccurrences)
                    {
                        if (occurrences > maxSecondResendDeltaOccurrences) maxSecondResendDeltaOccurrences = occurrences;
                    }
                    file << maxSecondResendDeltaOccurrences;

                    int mostOccurrences = 0;
                    int mostOccurrencesPosition = -1;
                    int mostOccurrencesDelta = 0;
                    auto countOccurrences = [&](const ResendPositionObservations &observations, int thisPosition)
                    {
                        for (const auto &[delta, occurrences] : observations.data)
                        {
                            if (occurrences >= mostOccurrences)
                            {
                                mostOccurrences = occurrences;
                                mostOccurrencesPosition = thisPosition;
                                mostOccurrencesDelta = thisPosition + delta;
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
                    countOccurrences(resendPositionMetadata.noResendObservations, 0);
                    for (const auto &secondResendMetadata : resendPositionMetadata.secondResendMetadata)
                    {
                        countOccurrences(secondResendMetadata.second.observations, secondResendMetadata.second.deltaToFirstResend);
                    }
                    if (mostOccurrences > 1)
                    {
                        file << ";y"
                             << ";" << mostOccurrences
                             << ";" << (resendPositionMetadata.deltaToNormalPathOptimalPosition + mostOccurrencesDelta)
                             << ";" << mostOccurrencesPosition;
                    }
                    else
                    {
                        file << ";;;;";
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
#if INSTRUMENTATION_ENABLED
        exploring = true;
#else
        exploring = false;
#endif
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
//        if (false) writePositionObservationMetadataFile(optimalGatherPositionsFilename(true), resourceToOptimalGatherPositions);
        writePositionObservationMetadataFile(optimalGatherPositionsFilename(true), resourceToOptimalGatherPositions);
//        writePositionObservationMetadataFile(tenDistancePositionsFilename(true), resourceTo10DistancePositions);
//        writePositionObservationMetadataFile(takeoverResendPositionsFilename(true), resourceToTakeoverResendPositions);
    }

    // Optimizes returning a resource
    void optimizeReturnOfResource(const MyWorker &worker, const MyUnit &depot, const Resource &resource)
    {
        // TODO
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

    bool isExploring()
    {
        return exploring;
    }

    void setExploring(bool newExploring)
    {
        exploring = newExploring;
    }
}