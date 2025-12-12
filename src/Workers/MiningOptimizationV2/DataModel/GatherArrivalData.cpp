#include "GatherArrivalData.h"

#include "OrderProcessTimer.h"

namespace MiningOptimization
{
    void GatherArrivalData::addDelayAfterAction(std::map<int, double> &delaysWithProbabilities,
                                                int orderProcessTimerAtArrival,
                                                int actionFrame,
                                                double baseProbability) const
    {
        // The base delay just uses the collision and facing patch data
        int baseDelay = (collision() ? 9 : 0) + (!facingTarget() ? 9 : 0);

        // For gather, the "action frame" is the frame that the worker transitions to MiningMinerals
        // If the order timer resets on this frame, we need to adjust the probabilities

        // If there is no order timer reset, just proceed normally
        if (!OrderProcessTimer::isResetFrame(actionFrame))
        {
            delaysWithProbabilities[baseDelay] += baseProbability;
            return;
        }

        // Otherwise simulate between 0 and 7 extra frames of delay
        for (int i = 0; i <= 7; i++)
        {
            delaysWithProbabilities[baseDelay + i] += (baseProbability * 0.125);
        }
    }
}
