#include "Solver.h"

#include "OrderProcessTimer.h"

#include "../DataModel/GatherArrivalData.h"

namespace MiningOptimization
{
    template <>
    bool Solver<GatherArrivalData>::canResendOnFrame(int frame, const std::set<int> &previousResendFrames) const
    {
        // Resends cannot take effect the frame before an order process timer reset, as this puts the worker in a weird state
        // The reason for this is that gather commands take two frames to work out. On the first frame, the command nullifies the order process
        // timer reset. But on the second frame, the order process timer reset will take effect and potentially delay the completion of the command
        // processing.
        if (OrderProcessTimer::isResetFrame(frame + BWAPI::Broodwar->getLatencyFrames() + 2)) return false;

        // Resends cannot be sent LF from each other, as this gives a Unit_Busy error
        return !previousResendFrames.contains(frame - BWAPI::Broodwar->getLatencyFrames());
    }

    template <>
    int Solver<GatherArrivalData>::transitionFramesToAction() const
    {
        // For gather, there is one transition frame while the worker is in WaitForMinerals
        return 1;
    }

    template class Solver<GatherArrivalData>;
}
