#include <BWAPI/SimulateGatherPathOptions.h>

#include <BWAPI/SimulateGatherPathResult.h>

#include "bwgame.h"

namespace BWAPI
{
    namespace
    {
        std::set<int> emptyResendFrames;
        std::unique_ptr<bwgame::state> emptyStartingState;
    }

    SimulateGatherPathOptions::SimulateGatherPathOptions()
        : resendFrames(emptyResendFrames)
        , startingState(emptyStartingState) {}

    SimulateGatherPathOptions::SimulateGatherPathOptions(const std::set<int> &resendFrames)
        : resendFrames(resendFrames)
        , startingState(emptyStartingState) {}

    SimulateGatherPathOptions::SimulateGatherPathOptions(const std::unique_ptr<bwgame::state> &startingState)
        : resendFrames(emptyResendFrames)
        , startingState(startingState) {}

    SimulateGatherPathOptions::SimulateGatherPathOptions(const std::set<int> &resendFrames, const std::unique_ptr<bwgame::state> &startingState)
        : resendFrames(resendFrames)
        , startingState(startingState) {}

    SimulateGatherPathOptions::~SimulateGatherPathOptions() = default;

    SimulateGatherPathOptions &SimulateGatherPathOptions::setForceAction(bool atArrival)
    {
        forceActionAtArrival = atArrival;
        forceActionAfterArrival = !atArrival;
        return *this;
    }

    SimulateGatherPathOptions &SimulateGatherPathOptions::setReturnStateAtStartOfNextPath()
    {
        returnStateAtStartOfNextPath = true;
        return *this;
    }

    SimulateGatherPathOptions &SimulateGatherPathOptions::switchToPatch(size_t _patchUnitIndex)
    {
        switchPatches = true;
        patchUnitIndex = _patchUnitIndex;
        return *this;
    }
}
