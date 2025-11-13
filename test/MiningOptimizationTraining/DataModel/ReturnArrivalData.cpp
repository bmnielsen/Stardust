#include "ReturnArrivalData.h"

namespace MiningOptimizationTraining
{
    std::ostream &operator<<(std::ostream &os, const ReturnExitSpeed exitSpeed)
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
