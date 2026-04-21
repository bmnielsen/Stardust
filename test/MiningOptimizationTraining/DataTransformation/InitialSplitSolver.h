#pragma once

#include "../DataModel/MapData.h"
#include "MiningOptimizationV2/DataModel/MapData.h"

namespace MiningOptimizationTraining
{
    class InitialSplitSolver
    {
    public:
        // Constructor used for single-worker gathering and return
        InitialSplitSolver(const InitialWorkerMapData &mapData,
                           const PositionAndVelocity &startPosition,
                           TilePosition firstPatch,
                           TilePosition secondPatch,
                           BWAPI::Race opponentRace)
                : mapData(mapData)
                , startPosition(startPosition)
                , firstPatch(firstPatch)
                , secondPatch(secondPatch)
                , opponentRace(opponentRace)
        {}

        // Executes the solver
        MiningOptimization::InitialSplitData execute();

    private:
        // We keep a reference to the map data so we can interpret some of the data correctly
        const InitialWorkerMapData &mapData;

        // The start position of this solver execution
        const PositionAndVelocity &startPosition;

        // The patches to use
        TilePosition firstPatch;
        TilePosition secondPatch;

        // The opponent race, where it is important to know if the opponent is Zerg, isn't Zerg, or is unknown
        BWAPI::Race opponentRace;
    };
}
