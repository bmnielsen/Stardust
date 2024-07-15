#include "WorkerOrderTimer.h"

#include <fstream>
#include <filesystem>
#include "Units.h"
#include "Workers.h"

#if INSTRUMENTATION_ENABLED
#define TRACK_MINING_EFFICIENCY true
#endif

namespace WorkerOrderTimer
{
    namespace
    {
        std::vector<std::string> dataLoadPaths = {
                "bwapi-data/AI/",
                "bwapi-data/read/",
                "bwapi-data/write/"
        };
        std::string dataWritePath = "bwapi-data/write/";

        std::map<Resource, std::set<PositionAndVelocity>> resourceToOptimalOrderPositions;
        std::map<MyWorker, std::map<int, PositionAndVelocity>> workerPositionHistory;

        std::string resourceOptimalOrderPositionsFilename(bool writing = false)
        {
            if (writing)
            {
                return (std::ostringstream() << dataWritePath << BWAPI::Broodwar->mapHash() << "_resourceOptimalOrderPositions.csv").str();
            }

            for (auto &path : dataLoadPaths)
            {
                auto filename = (std::ostringstream() << path << BWAPI::Broodwar->mapHash() << "_resourceOptimalOrderPositions.csv").str();
                if (std::filesystem::exists(filename)) return filename;
            }

            return "";
        }

        std::vector<int> readCsvLine(std::istream &str)
        {
            std::vector<int> result;

            std::string line;
            std::getline(str, line);

            try
            {
                std::stringstream lineStream(line);
                std::string cell;

                while (std::getline(lineStream, cell, ','))
                {
                    result.push_back(std::stoi(cell));
                }
            }
            catch (std::exception &ex)
            {
                Log::Get() << "Exception caught parsing optimal order position: " << ex.what() << "; line: " << line;
            }
            return result;
        }
    }

    void initialize()
    {
        resourceToOptimalOrderPositions.clear();
        workerPositionHistory.clear();

        // Attempt to open a CSV file storing the optimal positions found in previous matches on this map
        std::ifstream file;
        file.open(resourceOptimalOrderPositionsFilename());
        if (file.good())
        {
            try
            {
                // Read and parse each position
                while (true)
                {
                    auto line = readCsvLine(file);
                    if (line.size() != 6) break;

                    BWAPI::TilePosition tile(line[0], line[1]);
                    auto resource = Units::resourceAt(tile);
                    if (resource)
                    {
                        resourceToOptimalOrderPositions[resource].emplace(BWAPI::Position(line[2], line[3]), line[4], line[5]);
                    }
                }
            }
            catch (std::exception &ex)
            {
                Log::Get() << "Exception caught attempting to read optimal order positions: " << ex.what();
            }
        }
    }

    void write()
    {
        std::ofstream file;
        file.open(resourceOptimalOrderPositionsFilename(true), std::ofstream::trunc);

        for (auto &resourceAndOptimalOrderPositions : resourceToOptimalOrderPositions)
        {
            for (auto &optimalOrderPosition : resourceAndOptimalOrderPositions.second)
            {
                file << resourceAndOptimalOrderPositions.first->tile.x << ","
                     << resourceAndOptimalOrderPositions.first->tile.y << ","
                     << optimalOrderPosition << "\n";
            }
        }

        file.close();
    }

    bool optimizeStartOfMining(const MyWorker &worker, const Resource &resource)
    {
        auto resourceBwapiUnit = resource->getBwapiUnitIfVisible();
        if (!resourceBwapiUnit) return false;

        // Send a gather command when the worker has just delivered cargo
        // Occasionally from some patches a worker will wait an extra order timer round for no apparent reason, and this short-circuits that
        if (worker->lastDeliveredResource == currentFrame)
        {
            worker->gather(resourceBwapiUnit);
            return true;
        }

        // Mineral locking: make sure the worker doesn't switch targets
        if (worker->bwapiUnit->getOrderTarget() && worker->bwapiUnit->getOrderTarget()->getResources()
            && worker->bwapiUnit->getOrderTarget() != resourceBwapiUnit)
        {
            CherryVis::log(worker->id) << "Changed target patch";
            worker->gather(resourceBwapiUnit);
            return true;
        }

        // If another worker is currently mining this patch, resend the gather command when we expect the other worker to be finished mining in
        // 11+LF frames
        auto otherWorker = Workers::getOtherWorkerMining(resource, worker);
        if (otherWorker && otherWorker->exists() && otherWorker->bwapiUnit->getOrder() == BWAPI::Orders::MiningMinerals &&
            (otherWorker->bwapiUnit->getOrderTimer() + 7) == (11 + BWAPI::Broodwar->getLatencyFrames()))
        {
            // Exception: If we are not at the patch yet, and our last command was sent a long time ago,
            // we probably want to wait and allow our order timer optimization to send the command instead.
            int dist = resource->getDistance(worker);
            if (dist <= 20 || worker->lastCommandFrame >= (currentFrame - 20))
            {
                worker->gather(resourceBwapiUnit);
                return true;
            }
        }

        // Break out early if the distance is larger than we need to worry about
        auto dist = resource->getDistance(worker);
        if (dist > 100) return false;

        auto &positionHistory = workerPositionHistory[worker];
        auto &optimalOrderPositions = resourceToOptimalOrderPositions[resource];

        // If the worker is at the resource, record the optimal position
        if (dist == 0)
        {
            // The worker is at the resource, so if we have enough position history recorded,
            // record the optimal position
            int frame = currentFrame - BWAPI::Broodwar->getLatencyFrames() - 11;
            auto positionIt = positionHistory.find(frame);
            if (positionIt != positionHistory.end())
            {
                // Sometimes the probes will take different routes close to the mineral patch, perhaps because
                // of other nearby workers. This is OK, as we would rather send the order a frame late than a frame
                // early, but we still clear positions that are much too late.
                for (auto &frameAndPos : positionHistory)
                {
                    if (frameAndPos.first <= (frame + 2)) continue;
                    optimalOrderPositions.erase(frameAndPos.second);
                }

                optimalOrderPositions.insert(positionIt->second);
            }

            positionHistory.clear();

            return false;
        }

        PositionAndVelocity currentPositionAndVelocity(worker->bwapiUnit);

        // Check if this worker is at an optimal position to resend the gather order
        bool resent = false;
        if (worker->bwapiUnit->getOrder() == BWAPI::Orders::MoveToMinerals &&
            optimalOrderPositions.contains(currentPositionAndVelocity))
        {
            worker->gather(resourceBwapiUnit);
            resent = true;
        }

        // Record the worker's position
        positionHistory.emplace(std::make_pair(currentFrame, currentPositionAndVelocity));
        return resent;
    }
}
