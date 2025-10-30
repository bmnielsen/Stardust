#pragma once

#include "BWTest.h"
#include "ArrivalData.h"
#include "PositionOnPath.h"

namespace MiningOptimizationTraining
{
    enum class ResendChangesPath:uint8_t
    {
        Unknown,    // Have not explored this yet
        Yes,        // A resend could change the path (there are false positives)
        No          // A resend will not change the path
    };

    // Forward declaration so we can declare it as friend
    struct GatherObservations;

    // Encapsulates the result from a call to GatherObservations::leastObservedInPath
    struct LeastObservedInGatherPathResult
    {
        friend GatherObservations;

        // The exploration score of the best result so far
        // A score of 0.0 is neutral (the node has been explored as much as desired)
        // A positive score means the node has been overexplored
        // A negative score means the node has been underexplored
        double explorationScore;

        // The frames on which to resend for the best result so far
        std::set<int> resendFrames;

    private:
        // The number of explorable nodes further in the path
        int explorableNodes;

        LeastObservedInGatherPathResult(double explorationScore, std::set<int> resendFrames, int explorableNodes)
                : explorationScore(explorationScore)
                , resendFrames(std::move(resendFrames))
                , explorableNodes(explorableNodes)
        {}

        static LeastObservedInGatherPathResult NoResend(std::set<int> previousResends)
        {
            return LeastObservedInGatherPathResult{ 0.0, std::move(previousResends), 1 };
        }

        friend std::ostream &operator<<(std::ostream &out, const LeastObservedInGatherPathResult &obj)
        {
            // Use a separate buffer to format the text to avoid changing the configuration of the given stream
            std::ostringstream buf;
            buf << std::fixed << std::setprecision(3);
            buf << "Resend frames: ";
            std::string sep;
            for (int frame : obj.resendFrames)
            {
                buf << sep << frame;
                sep = ", ";
            }
            buf << "; score: " << obj.explorationScore;

            out << buf.str();
            return out;
        };
    };

    struct GatherArrivalObservations
    {
        // The arrival data (delay and facing patch) with their occurrences
        std::map<ArrivalData, uint32_t> arrivalToOccurrences;

        // How many arrivals had a collision
        uint32_t collisions = 0;

        // How many arrivals did not have a collision
        uint32_t nonCollisions = 0;

        void addArrivalObservation(ArrivalData arrivalData)
        {
            auto it = arrivalToOccurrences.find(arrivalData);
            if (it == arrivalToOccurrences.end())
            {
                arrivalToOccurrences[arrivalData] = 1;
                return;
            }
            if (it->second < UINT32_MAX) it->second++;
        }

        void addCollisionObservation(bool collision)
        {
            if ((collisions + nonCollisions) == UINT32_MAX) return;
            (collision ? collisions : nonCollisions)++;
        }

        [[nodiscard]] ArrivalData mostCommonArrivalData() const;
    };

    // This structure stores a node in a gather path
    // The path may branch because of a resend taking effect at this position or because of subpixel instability in the path
    struct GatherObservations
    {
    public:
        // The position and related data, stored in many different ways
        PositionOnPath pos;

        // How often this position has occurred in its path
        // For root nodes, how often it has been observed
        uint32_t occurrences = 0;

        // Whether a resend taking effect at this position could change the path.
        // This is determined by running an openbw pathfind and checking if the next waypoint differs from the current one. This does generate
        // false positives but is still useful for reducing the optimization search space
        ResendChangesPath canResendChangePath = ResendChangesPath::Unknown;

        // The arrival observations from this node when the path is not changed by a later gather command
        GatherArrivalObservations arrivalObservations;

        // The arrival observations when a resend takes effect here
        GatherArrivalObservations arrivalObservationsAfterResend;

        // All next positions seen from this position when the path has not been changed by a resend
        // Will be empty on the last node before arrival at the patch
        std::vector<GatherObservations> nextPositions;

        // All next positions seen from this position after a resend takes effect at this node
        // For training, we do not differentiate between resends that change the path and resends that do not
        std::vector<GatherObservations> nextPositionsAfterResend;

        // Whether this position is within the exploration window (the set of frames-before-arrival that we explore within)
        [[nodiscard]] bool withinExplorationWindow() const;

        // Gets the next gather observations on the path for a specific next position
        // Returns nullptr if there are no observations for that position
        GatherObservations *observationsForSpecificNextPosition(bool resendTakesEffectHere, const PositionOnPath &nextPos);

        // Gets the gather observations for the most likely next position on the path
        GatherObservations *observationsForMostLikelyNextPosition(bool resendTakesEffectHere);

        // Gets the gather observations for the most likely next position on the path (const version)
        [[nodiscard]] const GatherObservations *observationsForMostLikelyNextPosition(bool resendTakesEffectHere) const;

        // Looks forward in the path to find the branch that is least explored
        LeastObservedInGatherPathResult leastObservedInPath(
                std::set<int> &previousResends,
                int frame) const;
    };

    std::ostream &operator<<(std::ostream &os, const GatherObservations &gatherObservations);
}