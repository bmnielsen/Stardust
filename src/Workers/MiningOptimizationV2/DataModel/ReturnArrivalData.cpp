#include "ReturnArrivalData.h"

namespace MiningOptimization
{
    void ReturnArrivalData::addDelayAfterAction(std::map<int, double> &delaysWithProbabilities,
                                                int orderProcessTimerAtArrival,
                                                int actionFrame,
                                                double baseProbability) const
    {
        int delay;
        if (orderProcessTimerAtArrival == 0)
        {
            switch (exitSpeed())
            {
                case ReturnExitSpeed::Collision:
                    delay = 9;
                    break;
                case ReturnExitSpeed::Low:
                    delay = 0;
                    break;
                case ReturnExitSpeed::Medium:
                    delay = -2;
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

        delaysWithProbabilities[delay] += baseProbability;
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
            case ReturnExitSpeed::Medium:
                os << "Medium";
                break;
            case ReturnExitSpeed::High:
                os << "High";
                break;
        }
        return os;
    }
}
