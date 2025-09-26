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

    void readGatherPositionObservations(std::map<TilePosition, std::unordered_map<PositionAndVelocity, GatherPositionObservations>> &data);

    void readGatherPositionObservations(std::map<TilePosition, std::unordered_map<PositionAndVelocity, std::vector<uint8_t>>> &data);

    std::unique_ptr<GatherPositionObservations> deserializeGatherPositionObservations(const std::vector<uint8_t> &buffer);

    void read10DistanceObservations(std::map<TilePosition, std::unordered_set<PositionAndVelocity>> &data);

    void readReturnPositionObservations(std::map<TilePosition, std::unordered_map<PositionAndVelocity, ReturnPositionObservations>> &data);

    void readReturnPositionObservations(std::map<TilePosition, std::unordered_map<PositionAndVelocity, std::vector<uint8_t>>> &data);

    std::unique_ptr<ReturnPositionObservations> deserializeReturnPositionObservations(const std::vector<uint8_t> &buffer);

    void readResourceObservations(
            bool requireFull,
            std::map<TilePosition, ResourceObservations> &data);

    void writeFullGatherPositionObservations(std::map<TilePosition, std::unordered_map<PositionAndVelocity, GatherPositionObservations>> &data);

    void writeGatherPositionObservations(std::map<TilePosition, std::unordered_map<PositionAndVelocity, GatherPositionObservations>> &data);

    void write10DistanceObservations(std::map<TilePosition, std::unordered_set<PositionAndVelocity>> &data, bool maxCompression = false);

    void writeFullReturnPositionObservations(std::map<TilePosition, std::unordered_map<PositionAndVelocity, ReturnPositionObservations>> &data);

    void writeReturnPositionObservations(std::map<TilePosition, std::unordered_map<PositionAndVelocity, ReturnPositionObservations>> &data);

    void writeResourceObservations(
            bool minimized,
            std::map<TilePosition, ResourceObservations> &data,
            bool maxCompression = false);

    void reduceGatherData(std::map<TilePosition, std::unordered_map<PositionAndVelocity, GatherPositionObservations>> &data);
    void reduceReturnData(std::map<TilePosition, std::unordered_map<PositionAndVelocity, ReturnPositionObservations>> &data);
}
