#pragma once

#include "PositionAndVelocity.h"
#include "OrderProcessTimer.h"
#include "Resource.h"
#include <map>

namespace WorkerMiningOptimization
{
    struct ReturnArrivalObservations
    {
        std::unordered_map<int, int> arrivalDelayAndOccurrences;
        int collision = 0;
        int stopped = 0;
        int keptSpeed = 0;

        void add(int arrivalDelay)
        {
            arrivalDelayAndOccurrences[arrivalDelay]++;
        }

        [[nodiscard]] bool empty() const
        {
            return arrivalDelayAndOccurrences.empty();
        }

        [[nodiscard]] double expectedDeliveryDelay(int commandFrame) const;
    };

    // This is the structure we use to track observed positions and our track record using them
    struct ReturnPositionObservations
    {
    public:
        // Hash of the first position along this path after leaving the patch, only used for debugging
        // Value of 0 indicates the path didn't start at the patch
        uint32_t pathHash;

        // The position
        PositionAndVelocity pos;

        // All next positions seen from this position, with their count of observations
        // May be empty for the last position we consider in a path
        std::unordered_map<PositionAndVelocity, int> nextPositionAndOccurrences;

        // Observations for when no resend was sent here
        ReturnArrivalObservations noResendArrivalObservations;

        // Observations for when a resend was sent here
        ReturnArrivalObservations resendArrivalObservations;

        ReturnPositionObservations(uint32_t pathHash, PositionAndVelocity pos)
                : pathHash(pathHash)
                , pos(pos)
        {}

        ReturnPositionObservations(uint32_t pathHash, PositionAndVelocity pos, int arrival)
                : pathHash(pathHash)
                , pos(pos)
                , noResendArrivalObservations(ReturnArrivalObservations{{{arrival, 1}}})
        {}

        ReturnPositionObservations(
                uint32_t pathHash,
                PositionAndVelocity pos,
                std::unordered_map<PositionAndVelocity, int> &&nextPositionAndOccurrences,
                ReturnArrivalObservations &&noResendArrivalObservations,
                ReturnArrivalObservations &&resendArrivalObservations)
                : pathHash(pathHash)
                , pos(pos)
                , nextPositionAndOccurrences(nextPositionAndOccurrences)
                , noResendArrivalObservations(std::move(noResendArrivalObservations))
                , resendArrivalObservations(std::move(resendArrivalObservations))
        {}

        static void outputDataFileHeaderRow(std::ofstream &file);

        void outputToDataFile(std::ofstream &file, const Resource &resource) const;

        static bool parseFromDataFile(
                const std::vector<std::string> &line,
                std::map<Resource, std::unordered_map<PositionAndVelocity, ReturnPositionObservations>> &map,
                int lineNumber);
    };

    std::ostream &operator<<(std::ostream &os, const ReturnPositionObservations &returnPositionObservations);
}
