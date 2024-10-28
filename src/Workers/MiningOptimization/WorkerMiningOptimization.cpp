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

        // Metadata for positions used for optimizing approach to the patch
        std::map<Resource, std::unordered_map<PositionAndVelocity, PositionObservationMetadata>> resourceToOptimalGatherPositions;

        // Metadata for positions used for optimizing takeover
        std::map<Resource, std::unordered_map<PositionAndVelocity, PositionObservationMetadataForTakeoverResends>> resourceToTakeoverPositions;

        // Metadata for positions LF+1 frames before reaching 10 or less distance from the patch
        // Workers can try to switch to another patch if their chosen patch is being mined once they reach this distance, so we use these
        // positions to detect when we need to start resending gather commands to ensure mineral locking
        std::map<Resource, std::unordered_set<PositionAndVelocity>> resourceTo10DistancePositions;

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

        std::string takeoverPositionsFilename(bool writing = false)
        {
            auto filename = std::ostringstream()
                    << "takeoverpositions_" << BWAPI::Broodwar->mapHash()
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

        template <class T>
        void parsePositionObservationMetadataFile(const std::string &filename,
                                                  std::map<Resource, std::unordered_map<PositionAndVelocity, T>> &map)
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
                    if (lineNumber == 1 && !line.empty() && line[0] == "x") continue; // header row

                    if (T::parseFromDataFile(line, map, lineNumber)) break;

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

        template <class T>
        void writePositionObservationMetadataFile(const std::string &filename,
                                                  const std::map<Resource, std::unordered_map<PositionAndVelocity, T>> &map)
        {
            std::ofstream file;
            file.open(filename, std::ofstream::trunc);

            T::outputDataFileHeaderRow(file);

            int count = 0;
            for (const auto &[resource, gatherPositions] : map)
            {
                for (const auto &[resendPos, resendPositionMetadata] : gatherPositions)
                {
                    resendPositionMetadata.outputToDataFile(file, resource);
                    count++;
                }
            }

            file.close();
            Log::Get() << "Wrote " << count << " positions metadata to " << filename;
        }

        void parse10DistancePositionsFile(const std::string &filename,
                                          std::map<Resource, std::unordered_set<PositionAndVelocity>> &map)
        {
            map.clear();

            std::ifstream file;
            file.open(filename);
            if (!file.good()) return;

            int lineNumber = 0;
            try
            {
                // Each line is data for a single patch
                int count = 0;
                while (true)
                {
                    lineNumber++;

                    auto line = CsvTools::readNextLine(file);
                    if (line.size() < 3) break;
                    if (lineNumber == 1 && line[0] == "x") continue; // header row

                    BWAPI::TilePosition tile(std::stoi(line[0]), std::stoi(line[1]));
                    auto resource = Units::resourceAt(tile);
                    if (!resource) continue;

                    auto &positions = map[resource];
                    for (auto &posStr : CsvTools::tokenizeList(line[2], ','))
                    {
                        PositionAndVelocity pos;
                        if (!PositionAndVelocity::tryParse(posStr, pos))
                        {
                            Log::Get() << "Invalid position string at line " << lineNumber << "; skipping: " << posStr;
                            continue;
                        }

                        positions.insert(pos);
                        count++;
                    }
                }

                Log::Get() << "Read " << count << " 10-distance positions from " << filename;
            }
            catch (std::exception &ex)
            {
                Log::Get() << "Exception caught attempting to read 10-distance positions from " << filename
                           << " at line " << lineNumber << ": " << ex.what();
            }
        }

        void write10DistancePositionsFile(const std::string &filename,
                                          const std::map<Resource, std::unordered_set<PositionAndVelocity>> &map)
        {
            std::ofstream file;
            file.open(filename, std::ofstream::trunc);

            file << "x;y;positions\n";

            int count = 0;
            for (const auto &[resource, tenDistancePositions] : map)
            {
                if (tenDistancePositions.empty()) continue;

                file << resource->tile.x << ";"
                     << resource->tile.y << ";";

                std::string posSep;
                for (const auto &pos : tenDistancePositions)
                {
                    file << posSep << pos;
                    posSep = ",";
                    count++;
                }

                file << "\n";
            }

            file.close();
            Log::Get() << "Wrote " << count << " 10-distance positions to " << filename;
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
        parsePositionObservationMetadataFile(takeoverPositionsFilename(), resourceToTakeoverPositions);
        parse10DistancePositionsFile(tenDistancePositionsFilename(), resourceTo10DistancePositions);
    }

    void flushObservations()
    {
        flushStartOfMiningObservations(workerGatherStatuses);

        // TODO: Add return when ready
    }

    void write()
    {
        writePositionObservationMetadataFile(optimalGatherPositionsFilename(true), resourceToOptimalGatherPositions);
        writePositionObservationMetadataFile(takeoverPositionsFilename(true), resourceToTakeoverPositions);
        write10DistancePositionsFile(tenDistancePositionsFilename(true), resourceTo10DistancePositions);
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

    WorkerGatherStatus *gatherStatusFor(const MyWorker &worker)
    {
        auto workerStatusIt = workerGatherStatuses.find(worker);
        if (workerStatusIt == workerGatherStatuses.end()) return nullptr;

        return &workerStatusIt->second;
    }

    std::unordered_map<PositionAndVelocity, PositionObservationMetadata> &optimalGatherPositionsFor(const Resource &resource)
    {
        return resourceToOptimalGatherPositions[resource];
    }

    std::unordered_map<PositionAndVelocity, PositionObservationMetadataForTakeoverResends> &takeoverPositionsFor(const Resource &resource)
    {
        return resourceToTakeoverPositions[resource];
    }

    std::unordered_set<PositionAndVelocity> &tenDistancePositionsFor(const Resource &resource)
    {
        return resourceTo10DistancePositions[resource];
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