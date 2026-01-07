#include <BWAPI/SimulateGatherPathResult.h>

#include "bwgame.h"

namespace BWAPI
{
    SimulateGatherPathResult::SimulateGatherPathResult(std::vector<ExactPosition> positions,
                                                       ExactPosition actionPosition,
                                                       ExactPosition nextPathStartPosition,
                                                       uint64_t squaredSpeedEightFramesAlongNextPath,
                                                       std::unique_ptr<bwgame::state> stateAtStartOfNextPath)
            : positions(std::move(positions))
            , actionPosition(actionPosition)
            , nextPathStartPosition(nextPathStartPosition)
            , squaredSpeedEightFramesAlongNextPath(squaredSpeedEightFramesAlongNextPath)
            , stateAtStartOfNextPath(std::move(stateAtStartOfNextPath))
    {}

    SimulateGatherPathResult::~SimulateGatherPathResult() = default;
}
