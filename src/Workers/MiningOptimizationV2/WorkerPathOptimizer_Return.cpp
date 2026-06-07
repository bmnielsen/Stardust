#include "WorkerPathOptimizer.h"

namespace MiningOptimization
{
    template <>
    std::set<int> WorkerPathOptimizer<ReturnArrivalData>::takeoverActionFrames(int)
    {
        return {};
    }

    template <>
    void WorkerPathOptimizer<ReturnArrivalData>::useTakeoverFrame(int)
    {
    }

    template <>
    bool WorkerPathOptimizer<ReturnArrivalData>::isComplete()
    {
        return !worker->carryingResource;
    }

    template <>
    void WorkerPathOptimizer<ReturnArrivalData>::setStartOfPathFlags()
    {
        // Started at the end of last path if the worker got minerals on the last frame
        if (worker->carryingResource && worker->lastCarryingResourceChange == (currentFrame - 1))
        {
            setFlag(StatusFlags::StartedAtPreviousPathEnd);
        }
    }

    template <>
    bool WorkerPathOptimizer<ReturnArrivalData>::skipPathOptimization()
    {
        // Skip the first frame where the worker is resetting its movement path after gaining the resource
        if (worker->carryingResource
            && worker->lastCarryingResourceChange == currentFrame
            && worker->bwapiUnit->getOrder() == BWAPI::Orders::ResetCollision)
        {
            return true;
        }

        return false;
    }

    template <>
    void WorkerPathOptimizer<ReturnArrivalData>::initializeGatherTakeover()
    {
    }

    template <>
    bool WorkerPathOptimizer<ReturnArrivalData>::issueResend()
    {
        return worker->returnCargo();
    }

    template class WorkerPathOptimizer<ReturnArrivalData>;
}
