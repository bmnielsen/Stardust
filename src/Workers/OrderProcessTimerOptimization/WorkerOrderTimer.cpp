#include "WorkerOrderTimer.h"

#include <fstream>
#include <filesystem>
#include "Map.h"
#include "Units.h"

#include "../MineralLockingOptimization/MineralLockingOptimization.h"

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

        struct PositionAndVelocity
        {
            BWAPI::Position position;
            int velocityX;
            int velocityY;

            PositionAndVelocity(BWAPI::Position position, int velocityX, int velocityY)
                    : position(position), velocityX(velocityX), velocityY(velocityY) {}

            explicit PositionAndVelocity(const MyUnit &unit)
                    : position(unit->lastPosition)
                    , velocityX((int) (unit->bwapiUnit->getVelocityX() * 100.0))
                    , velocityY((int) (unit->bwapiUnit->getVelocityY() * 100.0)) {}
        };

        std::ostream &operator<<(std::ostream &out, const PositionAndVelocity &p)
        {
            out << p.position.x << "," << p.position.y << "," << p.velocityX << "," << p.velocityY;
            return out;
        }

        inline bool operator<(const PositionAndVelocity &a, const PositionAndVelocity &b)
        {
            return (a.position < b.position) ||
                   (a.position == b.position && a.velocityX < b.velocityX) ||
                   (a.position == b.position && a.velocityX == b.velocityX && a.velocityY < b.velocityY);
        }

        std::map<Resource, std::set<PositionAndVelocity>> resourceToOptimalOrderPositions;
        std::map<MyUnit, std::map<int, PositionAndVelocity>> workerPositionHistory;

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

    void gameEnd()
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

    void update()
    {
        // TODO: At some point track the order timers to see if we can predict their reset values
    }

    bool optimizeStartOfMining(Base *base, std::vector<std::tuple<MyWorker, MyUnit, Resource>> &workersAndDepotsAndResources)
    {
        return false;
    }

    void optimizeStartOfMining(const MyWorker &worker, const MyUnit &depot, const Resource &resource)
    {
        auto resourceBwapiUnit = resource->getBwapiUnitIfVisible();
        if (!resourceBwapiUnit) return;

        // Mineral locking
        if (worker->bwapiUnit->getOrderTarget() && worker->bwapiUnit->getOrderTarget()->getResources()
            && worker->bwapiUnit->getOrderTarget() != resourceBwapiUnit
            && worker->lastCommandFrame < (currentFrame - BWAPI::Broodwar->getLatencyFrames()))
        {
            worker->gather(resourceBwapiUnit);
            return;
        }

        // Break out early if the distance is larger than we need to worry about
        auto dist = resource->getDistance(worker);
        if (dist > 100) return;

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

            return;
        }

        PositionAndVelocity currentPositionAndVelocity(worker);

        // Check if this worker is at an optimal position to resend the gather order
        if (worker->bwapiUnit->getOrder() == BWAPI::Orders::MoveToMinerals &&
            optimalOrderPositions.contains(currentPositionAndVelocity))
        {
            auto bwapiUnit = resource->getBwapiUnitIfVisible();
            if (bwapiUnit)
            {
                worker->gather(resource->getBwapiUnitIfVisible());
            }
        }

        // Record the worker's position
        positionHistory.emplace(std::make_pair(currentFrame, currentPositionAndVelocity));
    }

    void optimizeReturnOfResource(const MyWorker &worker, const MyUnit &depot, const Resource &resource)
    {
        // Not implemented
    }

    std::map<MyWorker, std::tuple<Resource, Resource, std::optional<MiningOptimization::InitialSplitData>>> initialWorkerSplit()
    {
        return MineralLockingOptimization::initialWorkerSplit();
    }

    std::optional<int> averageRotationTimeFor(const Resource &resource)
    {
        return std::nullopt;
    }
}
