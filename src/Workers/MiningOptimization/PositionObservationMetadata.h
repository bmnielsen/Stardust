#pragma once

#include "PositionAndVelocity.h"
#include "OrderProcessTimer.h"
#include "Resource.h"
#include <map>

namespace WorkerMiningOptimization
{
    struct ResendPositionObservations
    {
        std::map<int, int> arrivalDelayAndOccurrences;
        int collisions = 0;
        int nonCollisions = 0;

        void add(int arrivalDelay)
        {
            arrivalDelayAndOccurrences[arrivalDelay]++;
        }

        [[nodiscard]] bool empty() const
        {
            return arrivalDelayAndOccurrences.empty();
        }

        [[nodiscard]] int mostCommonArrivalDelay() const;

        [[nodiscard]] double expectedMiningDelay(bool otherWorkerAssigned, int commandFrame) const;
    };

    struct SecondResendPositionObservationMetadata
    {
        PositionAndVelocity pos;
        std::unordered_map<PositionAndVelocity, int> next;
        int deltaToFirstResend = 0;
        ResendPositionObservations observations;
    };

    // This is the structure we use to track observed positions and our track record using them
    struct PositionObservationMetadata
    {
    public:
        // Hash of the first position along this path, only used for debugging
        // Value of 0 indicates the path didn't start at the depot
        // Value of UINT32_MAX indicates this position is earlier than our exploration horizon or we don't have full data for this path yet
        uint32_t pathHash;

        // The position
        PositionAndVelocity pos;

        // All next positions seen from this position, with their count of observations
        // May be empty for the last position we consider in a path
        std::unordered_map<PositionAndVelocity, int> next;

        // The offset between this resend position and the apparent optimal position if no resend had been issued
        // Defaults to 100 if we haven't observed this path without resends
        int deltaToNormalPathOptimalPosition;

        // Observations for when we send a resend here without a second resend
        ResendPositionObservations noSecondResendObservations;

        // Observations for second resends after resending at this position
        std::unordered_map<PositionAndVelocity, SecondResendPositionObservationMetadata> secondResendMetadata;

        // How many times the worker collides with the patch after mining when no resend is sent along this path
        int noResendCollisions = 0;

        // How many times the worker does not collide with the patch after mining when no resend is sent along this path
        int noResendNonCollisions = 0;

        // Whether a resend at this position changes the path. 1=Yes; -1=No; 0=Unknown
        int resendChangesPath = 0;

        bool addObservation(SecondResendPositionObservationMetadata* secondResendPositionData, int arrivalDelta);

        [[nodiscard]] std::vector<const SecondResendPositionObservationMetadata*> expectedPathAfterResend() const;

        [[nodiscard]] SecondResendPositionObservationMetadata* secondResendMetadataFor(const PositionAndVelocity *secondResendPosition)
        {
            if (!secondResendPosition) return nullptr;

            auto secondResendDataIt = secondResendMetadata.find(*secondResendPosition);
            return (secondResendDataIt == secondResendMetadata.end()) ? nullptr : &secondResendDataIt->second;
        }

        template<typename K>
        requires std::is_same_v<const K, const std::unordered_map<PositionAndVelocity, PositionObservationMetadata>>
        [[nodiscard]] auto followingPositionsIfStable(K &positionsData) const
        {
            using elem = std::remove_reference_t<decltype((positionsData.begin()->second))>;
            std::vector<elem*> result;
            const PositionObservationMetadata *current = this;
            while (!current->next.empty())
            {
                if (current->next.size() > 1)
                {
                    std::vector<elem*>{};
                }

                auto nextIt = positionsData.find(current->next.begin()->first);
                if (nextIt == positionsData.end()) break;

                result.push_back(&nextIt->second);

                current = &nextIt->second;
            }

            return result;
        }

        static void outputDataFileHeaderRow(std::ofstream &file);
        void outputToDataFile(std::ofstream &file, const Resource &resource) const;
        static bool parseFromDataFile(
                const std::vector<std::string> &line,
                std::map<Resource, std::unordered_map<PositionAndVelocity, PositionObservationMetadata>> &map,
                int lineNumber);
    };

    std::ostream &operator<<(std::ostream &os, const PositionObservationMetadata &optimalGatherPositionMetadata);
}
