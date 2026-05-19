#include "ReturnArrivalData.h"

#include "OrderProcessTimer.h"

namespace MiningOptimization
{
    void ReturnArrivalData::addDelayAfterAction(std::map<int, double> &delaysWithProbabilities,
                                                int orderProcessTimerAtArrival,
                                                int actionFrame,
                                                double baseProbability) const
    {
        int delay = 0;

        if (!facingTarget()) delay = 18; // Not really, but we really really want to avoid these paths since they mess with our timings so much

        if (orderProcessTimerAtArrival == 0)
        {
            auto speed = exitSpeed();
            if (speed == 0)
            {
                // Collision
                delay += 9;
            }
            else if (speed >= 102)
            {
                // Approx. 80% of max speed
                delay -= 4;
            }
            else if (speed >= 64)
            {
                // 50% of max speed
                delay -= 2;
            }
        }
        else if (collision())
        {
            delay += 9;
        }

        if (OrderProcessTimer::isResetFrame(actionFrame + 1))
        {
            double additionalDelayBaseProbability = baseProbability / 8.0;
            for (int additionalDelay = 0; additionalDelay < 8; additionalDelay++)
            {
                delaysWithProbabilities[delay + additionalDelay] += additionalDelayBaseProbability;
            }
        }
        else
        {
            delaysWithProbabilities[delay] += baseProbability;
        }
    }
}
