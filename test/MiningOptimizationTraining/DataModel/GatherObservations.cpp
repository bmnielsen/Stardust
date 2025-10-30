#include "GatherObservations.h"
#include "Configuration.h"

namespace MiningOptimizationTraining
{
    ArrivalData GatherArrivalObservations::mostCommonArrivalData() const
    {
        if (arrivalToOccurrences.empty()) return ArrivalData::nullopt();
        if (arrivalToOccurrences.size() == 1) return arrivalToOccurrences.begin()->first;
        uint32_t bestOccurrences = 0;
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
        return ((arrivalData.arrivalDelay() <= GATHER_EXPLORATION_WINDOW_START)
             && (arrivalData.arrivalDelay() >= GATHER_EXPLORATION_WINDOW_END));
    }

    GatherObservations *GatherObservations::observationsForSpecificNextPosition(bool resendTakesEffectHere, const PositionOnPath &nextPos)
    {
        auto &applicableNextPositions = (resendTakesEffectHere ? nextPositionsAfterResend : nextPositions);
        auto nextObservationIt = std::find_if(
                applicableNextPositions.begin(),
                applicableNextPositions.end(),
                [&nextPos](const GatherObservations &x)
                {
                    return x.pos == nextPos;
                });
        if (nextObservationIt == applicableNextPositions.end()) return nullptr;
        return &*nextObservationIt;
    }

    GatherObservations *GatherObservations::observationsForMostLikelyNextPosition(bool resendTakesEffectHere)
    {
        auto &applicableNextPositions = (resendTakesEffectHere ? nextPositionsAfterResend : nextPositions);

        // We keep the next positions vectors sorted by occurrences, so we can always just take the first
        return &*applicableNextPositions.begin();
    }

    const GatherObservations *GatherObservations::observationsForMostLikelyNextPosition(bool resendTakesEffectHere) const
    {
        auto &applicableNextPositions = (resendTakesEffectHere ? nextPositionsAfterResend : nextPositions);

        // We keep the next positions vectors sorted by occurrences, so we can always just take the first
        return &*applicableNextPositions.begin();
    }

    LeastObservedInGatherPathResult GatherObservations::leastObservedInPath( // NOLINT(*-no-recursion)
            std::set<int> &previousResends,
            int frame) const
    {
        // Our goal is to roughly equally explore the no-resend path and resending at each node in the exploration window (ignoring nodes where
        // resends do not affect the path).
        // To calculate this, we do the following:
        // - Explore forwards along the most likely path up to the end of the exploration window
        // - While backtracking, count the number of nodes where resends change the path (these are the nodes that need exploration)
        // - At each node, we score how explored it is by comparing the no-resend observations to resend observations, taking into account that each
        //   explored node later in the path will be observed as a no-resend here
        // - If we are not at the maximum resend depth, we also explore the resend path in the same way as above

        // Get the next likely position
        bool resendTakesEffectHere = previousResends.contains(frame - BWAPI::Broodwar->getLatencyFrames());
        auto next = observationsForMostLikelyNextPosition(resendTakesEffectHere);

        // Explore forwards by scoring the next node
        // If it doesn't exist or is outside the window, seed with our default value
        LeastObservedInGatherPathResult nextResult = (next && next->withinExplorationWindow())
            ? next->leastObservedInPath(previousResends, frame + 1)
            : LeastObservedInGatherPathResult::NoResend(previousResends);

        // If we are at a node that doesn't have resends changing the path, or is not in the observation window, we just return the next result
        if (canResendChangePath != ResendChangesPath::Yes || !withinExplorationWindow())
        {
            return nextResult;
        }

        // Extend the set of previous resends with resending at this node
        std::set<int> resends = previousResends;
        resends.insert(frame - BWAPI::Broodwar->getLatencyFrames());

        // If we can resend from here, explore forward along the resend path
        // The goal of this is to populate resendResult with the number of explorable nodes on the resend path and the best node to explore along it
        LeastObservedInGatherPathResult resendResult =
            (!resendTakesEffectHere                                     // If a resend takes effect here, we're already exploring the resend path
                && previousResends.size() < (GATHER_RESEND_LIMIT - 1)   // We must be able to send more resends
                && !nextPositionsAfterResend.empty())                   // We must have observed the path at least once
            ? observationsForMostLikelyNextPosition(true)->leastObservedInPath(resends, frame + 1)
            : LeastObservedInGatherPathResult::NoResend(std::move(resends));

        // Score resending here, unless it is impossible to resend here because of Unit_Busy
        if (previousResends.contains(frame - BWAPI::Broodwar->getLatencyFrames() * 2))
        {
            resendResult.explorationScore = 1.0;
        }
        else
        {
            // The desired ratio compares the explorable nodes along the no-resend and resend paths
            double desiredRatio = (double)resendResult.explorableNodes / (double)nextResult.explorableNodes;

            // The actual ratio is the sum of the resend occurrences divided by the sum of the no resend occurrences
            auto totalOccurrences = [](const std::vector<GatherObservations> &nextPositions)
            {
                uint32_t total = 0;
                for (auto &next : nextPositions)
                {
                    total += next.occurrences;
                }
                return total;
            };
            uint32_t totalNoResend = totalOccurrences(nextPositions);
            double actualRatio = (totalNoResend == 0) ? 1.0 : ((double)totalOccurrences(nextPositionsAfterResend) / (double)totalNoResend);

            // The score is the difference between the two scaled by desired ratio
            resendResult.explorationScore = (actualRatio - desiredRatio) / desiredRatio;
        }

        // The explorable nodes to return from here is the combination of those on the no-resend path with those on the resend path
        int explorableNodes = nextResult.explorableNodes + resendResult.explorableNodes;

        // Return the next result if it is better
        if (nextResult.explorationScore < resendResult.explorationScore)
        {
            nextResult.explorableNodes = explorableNodes;
            return nextResult;
        }

        // Otherwise return the resend result, which has the resends set to whichever combination is least explored along the resend path
        resendResult.explorableNodes = explorableNodes;
        return resendResult;
    }

    std::ostream &operator<<(std::ostream &os, const GatherObservations &gatherObservations)
    {
        os << gatherObservations.pos << " * " << gatherObservations.occurrences;
        return os;
    }
}
