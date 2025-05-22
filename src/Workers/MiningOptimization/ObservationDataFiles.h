#pragma once

#include "Common.h"

#include "TilePosition.h"
#include "PositionAndVelocity.h"
#include "GatherPositionObservations.h"
#include "ReturnPositionObservations.h"
#include "ResourceObservations.h"

namespace WorkerMiningOptimization::ObservationDataFiles
{
    struct GameParameters
    {
        std::string mapHash;
        std::string exportMapHash;
        int latencyFrames;
        int gatherExploreBefore;
        int gatherExploreAfter;
        int returnExploreBefore;
        int returnExploreAfter;
    };

    void overrideGameParameters(GameParameters gameParameters);
    GameParameters getGameParameters();

    void readGatherPositionObservations(
            bool requireFull,
            std::map<TilePosition, std::unordered_map<PositionAndVelocity, GatherPositionObservations>> &data);

    void read10DistanceObservations(std::map<TilePosition, std::unordered_set<PositionAndVelocity>> &data);

    void readReturnPositionObservations(
            bool requireFull,
            std::map<TilePosition, std::unordered_map<PositionAndVelocity, ReturnPositionObservations>> &data);

    void readResourceObservations(
            bool requireFull,
            std::map<TilePosition, ResourceObservations> &data);

    void writeGatherPositionObservations(
            bool minimized,
            std::map<TilePosition, std::unordered_map<PositionAndVelocity, GatherPositionObservations>> &data,
            bool maxCompression = false);

    void write10DistanceObservations(std::map<TilePosition, std::unordered_set<PositionAndVelocity>> &data, bool maxCompression = false);

    void writeReturnPositionObservations(
            bool minimized,
            std::map<TilePosition, std::unordered_map<PositionAndVelocity, ReturnPositionObservations>> &data,
            bool maxCompression = false);

    void writeResourceObservations(
            bool minimized,
            std::map<TilePosition, ResourceObservations> &data,
            bool maxCompression = false);

    void reduceGatherData(std::map<TilePosition, std::unordered_map<PositionAndVelocity, GatherPositionObservations>> &data);
    void reduceReturnData(std::map<TilePosition, std::unordered_map<PositionAndVelocity, ReturnPositionObservations>> &data);
}
