#include "Solver.h"

#include "OrderProcessTimer.h"

#include "../DataModel/GatherArrivalData.h"

namespace MiningOptimization
{
    template <>
    bool Solver<GatherArrivalData>::canResendOnFrame(int frame, std::set<int> &previousResendFrames)
    {
        // Resends cannot be sent LF+1 before an order process timer reset, as this puts the worker in a weird state
        if (OrderProcessTimer::isResetFrame(frame + BWAPI::Broodwar->getLatencyFrames() + 1)) return false;

        // Resends cannot be send LF from each other, as this gives a Unit_Busy error
        return !previousResendFrames.contains(frame - BWAPI::Broodwar->getLatencyFrames());
    }

    template class Solver<GatherArrivalData>;
}
