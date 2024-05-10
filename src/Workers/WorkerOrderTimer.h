#pragma once

#include "Common.h"
#include "MyUnit.h"
#include "Resource.h"

namespace WorkerOrderTimer
{
    struct PositionAndVelocity
    {
        BWAPI::Position position;
        int velocityX;
        int velocityY;

        PositionAndVelocity(BWAPI::Position position, int velocityX, int velocityY)
                : position(position), velocityX(velocityX), velocityY(velocityY) {}

        explicit PositionAndVelocity(BWAPI::Unit bwapiUnit)
                : position(bwapiUnit->getPosition())
                , velocityX((int) (bwapiUnit->getVelocityX() * 100.0))
                , velocityY((int) (bwapiUnit->getVelocityY() * 100.0)) {}

        friend std::ostream &operator<<(std::ostream &out, const PositionAndVelocity &p)
        {
            out << p.position.x << "," << p.position.y << "," << p.velocityX << "," << p.velocityY;
            return out;
        }

        friend bool operator<(const PositionAndVelocity &a, const PositionAndVelocity &b)
        {
            return (a.position < b.position) ||
                   (a.position == b.position && a.velocityX < b.velocityX) ||
                   (a.position == b.position && a.velocityX == b.velocityX && a.velocityY < b.velocityY);
        }
    };

    struct GatheringMineralsWorker
    {
        std::map<int, PositionAndVelocity> positionHistory;
        bool hadMineralsLastFrame;
        std::vector<int> mineralsReceivedFrames;
        std::vector<int> mineralsDeliveredFrames;

        GatheringMineralsWorker(BWAPI::Unit worker) : hadMineralsLastFrame(worker->isCarryingMinerals()) {}

        void update(BWAPI::Unit worker)
        {
            positionHistory.emplace(currentFrame, worker);

            bool hasMinerals = worker->isCarryingMinerals();
            if (hasMinerals != hadMineralsLastFrame)
            {
                (hasMinerals ? mineralsReceivedFrames : mineralsDeliveredFrames).push_back(currentFrame);
                hadMineralsLastFrame = hasMinerals;
            }
        }
    };

    struct MineralPatch
    {
        int tileX;
        int tileY;
        std::set<PositionAndVelocity> optimalGatherPositions;
        std::set<PositionAndVelocity> blacklistedGatherPositions;
    };

    void initialize();

    void write();

    void update();

    // Resends gather orders to optimize the worker order timer. Returns whether an order was sent to the worker.
    bool optimizeStartOfMining(const MyUnit &worker, const Resource &resource);

    // Resends return or gather orders to optimize delivery of resources at the depot. Returns whether an order was sent to the worker.
    bool optimizeReturn(const MyUnit &worker, const Resource &resource, const Unit &depot);

    void writeInstrumentation();

    double getEfficiency();
}
