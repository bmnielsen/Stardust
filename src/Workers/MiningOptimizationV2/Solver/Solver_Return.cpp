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

    template <>
    int Solver<ReturnArrivalData>::transitionFramesToAction() const
    {
        // Delivery has no transition frame, it just delivers as soon as the order process timer is 0 after arrival
        return 0;
    }

    template class Solver<ReturnArrivalData>;
}
