#include "WorkerPathOptimizer.h"

namespace MiningOptimization
{
    template <typename ObservationType>
    void WorkerPathOptimizer<ObservationType>::optimize()
    {
        if (skipPathOptimization()) return;

        // Reset if the worker hasn't been optimized last frame
        if (lastProcessedFrame != (currentFrame - 1)) reset();
        lastProcessedFrame = currentFrame;

        // Extract the path when we reach a root node
        if (!pathBeingFollowed)
        {
            auto it = pathData.find(PositionAndVelocity(worker));
            if (it != pathData.end())
            {
                pathBeingFollowed = std::make_unique<Path<ObservationType>>(it->second.get());
#if VERBOSE_MINING_LOGGING
                CherryVis::log(worker->id) << "Captured path";
#endif
            }
        }

        // TODO: Validate path

        // TODO: Plan path

        // Send a planned resend for this frame
        if (plannedResendFrames.contains(currentFrame))
        {
            auto result = issueResend();
            if (result)
            {
                executedResendFrames.insert(currentFrame);
#if VERBOSE_MINING_LOGGING
                CherryVis::log(worker->id) << "Issued planned resend";
            }
            else
            {
                Log::Get() << "Failed to issue planned resend for " << worker->id << " @ " << worker->getTilePosition() << ": "
                           << BWAPI::Broodwar->getLastError();
                CherryVis::log(worker->id) << "Failed to issue planned resend; last error " << BWAPI::Broodwar->getLastError();
#endif
            }
        }
    }

    template class WorkerPathOptimizer<GatherArrivalData>;
    template class WorkerPathOptimizer<ReturnArrivalData>;
}
