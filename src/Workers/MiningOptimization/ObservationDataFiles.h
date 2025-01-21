#pragma once

#include "Common.h"

#include "TilePosition.h"
#include "PositionAndVelocity.h"
#include "GatherPositionObservations.h"
#include "ReturnPositionObservations.h"

namespace WorkerMiningOptimization
{
    void overrideGameParameters(const std::string &mapHash,
                                int latencyFrames,
                                int gatherExploreBefore,
                                int gatherExploreAfter,
                                int returnExploreBefore,
                                int returnExploreAfter);

    void readGatherPositionObservations(
            bool preferFull,
            std::map<TilePosition, std::unordered_map<PositionAndVelocity, GatherPositionObservations>> &data);

    void read10DistanceObservations(std::map<TilePosition, std::unordered_set<PositionAndVelocity>> &data);

    void readReturnPositionObservations(
            bool preferFull,
            std::map<TilePosition, std::unordered_map<PositionAndVelocity, ReturnPositionObservations>> &data);

    void writeGatherPositionObservations(
            bool minimized,
            std::map<TilePosition, std::unordered_map<PositionAndVelocity, GatherPositionObservations>> &data);

    void write10DistanceObservations(std::map<TilePosition, std::unordered_set<PositionAndVelocity>> &data);

    void writeReturnPositionObservations(
            bool minimized,
            std::map<TilePosition, std::unordered_map<PositionAndVelocity, ReturnPositionObservations>> &data);

    void reduceGatherData(std::map<TilePosition, std::unordered_map<PositionAndVelocity, GatherPositionObservations>> &data);
}
