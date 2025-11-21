#include "WorkerPathOptimizer.h"

namespace MiningOptimization
{
    template <>
    bool WorkerPathOptimizer<ReturnArrivalData>::skipPathOptimization()
    {
        // For return, nothing is needed here since we follow the path right up until the worker delivers minerals and starts
        // on its next gather path
        return false;
    }

    template <>
    bool WorkerPathOptimizer<ReturnArrivalData>::issueResend()
    {
        return worker->returnCargo();
    }

    template class WorkerPathOptimizer<ReturnArrivalData>;
}
