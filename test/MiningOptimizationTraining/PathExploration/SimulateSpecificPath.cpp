#include "ExploreStartPositionsModule.h"

#include <BWAPI/SimulateGatherPathOptions.h>
#include <BWAPI/SimulateGatherPathResult.h>


namespace MiningOptimizationTraining
{
    template <>
    void ExploreStartPositionsModule<SimulateSpecificPath>::initializeStartPositions()
    {
        BWAPI::Unit patchUnit;
        for (auto unit : BWAPI::Broodwar->getNeutralUnits())
        {
            if (unit->getType().isMineralField() && unit->getTilePosition() == BWAPI::TilePosition(1, 6))
            {
                patchUnit = unit;
            }
        }

        startPositions.emplace_back(InitializeStartPosition{
                BWAPI::ExactPosition{(107 << 8) + 248, (203 << 8) + 248, -68, 0, 0},
                patchUnit});
    }

    template <>
    void ExploreStartPositionsModule<SimulateSpecificPath>::explore(SimulateSpecificPath &startPosition)
    {
        auto preparedReturnPath = prepareReturnPath(startPosition, initialStateWithNoCannons);
        if (!preparedReturnPath) return;

        auto returnResult = simWorker->simulateGatherPath(
                BWAPI::SimulateGatherPathOptions({}, preparedReturnPath->returnPathState));
        if (!returnResult)
        {
            Log::Get() << "Failed to simulate";
            return;
        }

        Log::Get() << startPosition.pos;
        for (auto &pos : returnResult->positions)
        {
            Log::Get() << pos;
        }
    }
}
