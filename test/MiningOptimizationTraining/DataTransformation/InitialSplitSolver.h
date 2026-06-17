#pragma once

#include "../DataModel/MapData.h"
#include "MiningOptimizationV2/DataModel/MapData.h"

namespace MiningOptimizationTraining
{
    template <typename StartPositionState>
    class ExploreStartPositionsModule;
    struct ExploreInitialWorkerStartPosition;

    class InitialSplitSolver
    {
    public:
        friend class ExploreStartPositionsModule<ExploreInitialWorkerStartPosition>;

        struct PathResult
        {
            BWAPI::ExactPosition startPosition;
            int startFrame;
            int frameDelay;
            std::vector<InitialWorkerComputePathResult> pathResults;
            std::set<int> resends;
            const PathResult* previousPathResult;

            [[nodiscard]] int worstActionFrame() const;

            [[nodiscard]] int worstActionFrameAndDelay() const;

            [[nodiscard]] bool equivalentTo(const PathResult &other) const;

            [[nodiscard]] MiningOptimization::InitialSplitRotation toInitialSplitRotation(
                const std::map<int, unsigned int> *orderProcessTimerResetValues) const;
        };

        // Constructor used for single-worker gathering and return
        InitialSplitSolver(const InitialWorkerMapData &mapData,
                           PositionAndVelocity startPosition,
                           TilePosition firstPatch,
                           TilePosition secondPatch,
                           BWAPI::Race opponentRace)
                : mapData(mapData)
                , startPosition(startPosition)
                , firstPatch(firstPatch)
                , secondPatch(secondPatch)
                , opponentRace(opponentRace)
        {
            exactStartPosition = BWAPI::ExactPosition((uint32_t)startPosition.x * 256,
                                                                   (uint32_t)startPosition.y * 256,
                                                                   startPosition.heading,
                                                                   0,
                                                                   0);

            EXPECT_TRUE(mapData.startingWorkerPositionToOrderProcessTimerReset.contains(startPosition))
                                << "No order process timer reset value data for start position " << startPosition;

            bool opponentRaceUnknown = (
                opponentRace != BWAPI::Races::Protoss &&
                opponentRace != BWAPI::Races::Terran &&
                opponentRace != BWAPI::Races::Zerg);

            for (const auto &resetData : mapData.startingWorkerPositionToOrderProcessTimerReset.at(startPosition))
            {
                // If we don't know the opponent's race, we multiply the weighting to indicate that getting either protoss or terran is twice as
                // likely as getting zerg
                if (opponentRaceUnknown)
                {
                    orderProcessTimerResetValues[resetData.value] += resetData.opponentStartLocationsCount * (resetData.opponentIsZerg ? 1 : 2);
                    continue;
                }

                // Skip this value if it doesn't match the known race
                if (resetData.opponentIsZerg && (opponentRace == BWAPI::Races::Protoss || opponentRace == BWAPI::Races::Terran)) continue;
                if (!resetData.opponentIsZerg && (opponentRace == BWAPI::Races::Zerg)) continue;

                orderProcessTimerResetValues[resetData.value] += resetData.opponentStartLocationsCount;
            }

            if (orderProcessTimerResetValues.empty())
            {
                Log::Get() << "ERROR: Solver initialized with no valid order process timer reset values; does the initialization need to be run?";
            }
        }

        // Executes the solver
        std::optional<MiningOptimization::InitialSplitData> execute();

    private:
        // We keep a reference to the map data so we can interpret some of the data correctly
        const InitialWorkerMapData &mapData;

        // The start position of this solver execution
        PositionAndVelocity startPosition;
        BWAPI::ExactPosition exactStartPosition;

        // The patches to use
        TilePosition firstPatch;
        TilePosition secondPatch;

        // The opponent race, where it is important to know if the opponent is Zerg, isn't Zerg, or is unknown
        BWAPI::Race opponentRace;

        // The possible order process timer reset values and their occurrence rates
        std::map<int, unsigned int> orderProcessTimerResetValues;

        // These data structures store the results as the solver executes
        std::vector<PathResult> firstGatherPathResults;
        std::vector<PathResult> firstReturnPathResults;
        std::map<PathResult*, std::vector<std::vector<PathResult>>> secondGatherPathResults;
        std::vector<std::pair<const PathResult*, std::map<int, std::pair<PathResult, PathResult>>>> secondReturnPathResults;

        // These methods execute each phase of the solver
        void executeFirstGather();
        void executeFirstReturn();
        void executeSecondGather();
        void executeSecondReturn();
    };
}
