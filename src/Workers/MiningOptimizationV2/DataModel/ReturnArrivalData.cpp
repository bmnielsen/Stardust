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
        if (orderProcessTimerAtArrival == 0)
        {
            switch (exitSpeed())
            {
                case ReturnExitSpeed::NotFacingDepot:
                    delay = 18; // Not really, but we really really want to avoid these paths since they mess with our timings so much
                    break;
                case ReturnExitSpeed::Collision:
                    delay = 9;
                    break;
                case ReturnExitSpeed::Low:
                    delay = 0;
                    break;
                case ReturnExitSpeed::High:
                    delay = -4;
                    break;
            }
        }
        else if (collision)
        {
            delay = 9;
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

    std::ostream &operator<<(std::ostream &os, const ReturnExitSpeed &exitSpeed)
    {
        switch (exitSpeed)
        {
            case ReturnExitSpeed::Collision:
                os << "Collision";
                break;
            case ReturnExitSpeed::Low:
                os << "Low";
                break;
            case ReturnExitSpeed::High:
                os << "High";
                break;
            case ReturnExitSpeed::NotFacingDepot:
                os << "NotFacingDepot";
                break;
        }
        return os;
    }
}
