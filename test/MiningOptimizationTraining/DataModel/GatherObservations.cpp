
#include "GatherObservations.h"

namespace MiningOptimizationTraining
{
    ArrivalData GatherArrivalObservations::mostCommonArrivalData() const
    {
        if (arrivalToOccurrences.empty()) return ArrivalData::nullopt();
        if (arrivalToOccurrences.size() == 1) return arrivalToOccurrences.begin()->first;
        int bestOccurrences = 0;
        ArrivalData bestArrivalData = ArrivalData::nullopt();
        for (const auto &[arrivalData, occurrences] : arrivalToOccurrences)
        {
            if (occurrences > bestOccurrences)
            {
                bestOccurrences = occurrences;
                bestArrivalData = arrivalData;
            }
        }
        return bestArrivalData;
    }

    bool GatherObservations::withinExplorationWindow() const
    {
        auto arrivalData = arrivalObservations.mostCommonArrivalData();
        return ((arrivalData.arrivalDelay() <= EXPLORATION_WINDOW_START)
             && (arrivalData.arrivalDelay() >= EXPLORATION_WINDOW_END));
    }

    std::ostream &operator<<(std::ostream &os, const GatherObservations &gatherObservations)
    {
        os << gatherObservations.pos << " * " << gatherObservations.occurrences;
        return os;
    }
}
