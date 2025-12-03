#include "Solver.h"

#include "../DataModel/ReturnArrivalData.h"

namespace MiningOptimization
{
    template <>
    bool Solver<ReturnArrivalData>::canResendOnFrame(int frame, const std::set<int> &previousResendFrames) const
    {
        // For return, there are no frame timing limitations
        return true;
    }

    template class Solver<ReturnArrivalData>;
}
