#include <BWAPI/SimulateGatherPathResult.h>

#include "bwgame.h"

namespace BWAPI
{
    SimulateGatherPathResult::SimulateGatherPathResult(int startFrame,
                                                       int arrivalFrame,
                                                       int actionFrame,
                                                       int lastOrderProcessTimerOverrideFrame,
                                                       std::vector<ExactPosition> positions,
                                                       ExactPosition actionPosition,
                                                       ExactPosition nextPathStartPosition,
                                                       uint64_t squaredSpeedEightFramesAlongNextPath,
                                                       std::unique_ptr<bwgame::state> stateAtStartOfNextPath)
            : startFrame(startFrame)
            , arrivalFrame(arrivalFrame)
            , actionFrame(actionFrame)
            , lastOrderProcessTimerOverrideFrame(lastOrderProcessTimerOverrideFrame)
            , positions(std::move(positions))
            , actionPosition(actionPosition)
            , nextPathStartPosition(nextPathStartPosition)
            , squaredSpeedEightFramesAlongNextPath(squaredSpeedEightFramesAlongNextPath)
            , stateAtStartOfNextPath(std::move(stateAtStartOfNextPath))
    {}

    SimulateGatherPathResult::~SimulateGatherPathResult() = default;
}
