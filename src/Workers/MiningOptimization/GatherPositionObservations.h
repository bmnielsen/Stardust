#pragma once

#include "TilePosition.h"
#include "PositionAndVelocity.h"
#include "OrderProcessTimer.h"
#include "Resource.h"
#include <map>

namespace WorkerMiningOptimization
{
    struct GatherResendArrivalObservations
    {
        std::unordered_map<int, int> arrivalDelayAndOccurrences;
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

        [[nodiscard]] double expectedMiningDelay(int commandFrame) const;
    };

    struct SecondResendGatherPositionObservations
    {
        PositionAndVelocity pos;
        int deltaToFirstResend = 0;
        std::unordered_map<PositionAndVelocity, int> nextPositionAndOccurrences;
        GatherResendArrivalObservations arrivalObservations;
    };

    // This is the structure we use to track observed positions and our track record using them
    struct GatherPositionObservations
    {
    public:
        // Hash of the first position along this path, only used for debugging
        // Value of 0 indicates the path didn't start at the depot
        // Value of UINT32_MAX indicates this position is earlier than our exploration horizon or we don't have full data for this path yet
        uint32_t pathHash;

        // The position
        PositionAndVelocity pos;

        // The offset between this resend position and the apparent optimal position if no resend had been issued
        // For positions with unstable following paths this can differ, so we store all observed values with their occurrences
        // May be empty if we haven't observed a no-resend path with this position yet
        std::unordered_map<int, int> deltaToBenchmarkAndOccurrences;

        // All next positions seen from this position, with their count of observations
        // May be empty for the last position we consider in a path
        std::unordered_map<PositionAndVelocity, int> nextPositionAndOccurrences;

        // Observations for when we send a resend here without a second resend
        GatherResendArrivalObservations noSecondResendArrivalObservations;

        // Observations for second resends after resending at this position
        std::unordered_map<PositionAndVelocity, SecondResendGatherPositionObservations> secondResendObservations;

        // How many times the worker collides with the patch after mining when no resend is sent along this path
        int noResendCollisions = 0;

        // How many times the worker does not collide with the patch after mining when no resend is sent along this path
        int noResendNonCollisions = 0;

        // Whether a resend at this position changes the path. 1=Yes; -1=No; 0=Unknown
        int resendChangesPath = 0;

        GatherPositionObservations(uint32_t pathHash, PositionAndVelocity pos)
                : pathHash(pathHash)
                , pos(pos)
        {}

        GatherPositionObservations(uint32_t pathHash, PositionAndVelocity pos, int deltaToBenchmarkAndOccurrences)
                : pathHash(pathHash)
                , pos(pos)
                , deltaToBenchmarkAndOccurrences({{deltaToBenchmarkAndOccurrences, 1}})
        {}

        GatherPositionObservations(
                uint32_t pathHash,
                PositionAndVelocity pos,
                std::unordered_map<int, int> &&deltaToBenchmarkAndOccurrences,
                std::unordered_map<PositionAndVelocity, int> &&nextPositionAndOccurrences,
                GatherResendArrivalObservations &&noSecondResendArrivalObservations,
                std::unordered_map<PositionAndVelocity, SecondResendGatherPositionObservations> &&secondResendObservations,
                int noResendCollisions = 0,
                int noResendNonCollisions = 0,
                int resendChangesPath = 0)
                : pathHash(pathHash)
                , pos(pos)
                , deltaToBenchmarkAndOccurrences(std::move(deltaToBenchmarkAndOccurrences))
                , nextPositionAndOccurrences(std::move(nextPositionAndOccurrences))
                , noSecondResendArrivalObservations(std::move(noSecondResendArrivalObservations))
                , secondResendObservations(std::move(secondResendObservations))
                , noResendCollisions(noResendCollisions)
                , noResendNonCollisions(noResendNonCollisions)
                , resendChangesPath(resendChangesPath)
        {}

        [[nodiscard]] double averageDeltaToBenchmark() const;

        [[nodiscard]] int probableDeltaToBenchmark() const;

        [[nodiscard]] int largestDeltaToBenchmark() const;

        bool addArrivalObservation(SecondResendGatherPositionObservations *secondResendPositionData, int arrivalDelta);

        [[nodiscard]] SecondResendGatherPositionObservations *secondResendObservationsFor(const PositionAndVelocity *secondResendPosition)
        {
            if (!secondResendPosition) return nullptr;

            auto secondResendDataIt = secondResendObservations.find(*secondResendPosition);
            return (secondResendDataIt == secondResendObservations.end()) ? nullptr : &secondResendDataIt->second;
        }

        template<typename K>
        requires std::is_same_v<const K, const std::unordered_map<PositionAndVelocity, GatherPositionObservations>>
        [[nodiscard]] auto followingPositionsIfStable(K &positionsData) const
        {
            using elem = std::remove_reference_t<decltype((positionsData.begin()->second))>;
            std::vector<elem *> result;
            const GatherPositionObservations *current = this;
            while (!current->nextPositionAndOccurrences.empty())
            {
                if (current->nextPositionAndOccurrences.size() > 1)
                {
                    std::vector<elem *>{};
                }

                auto nextIt = positionsData.find(current->nextPositionAndOccurrences.begin()->first);
                if (nextIt == positionsData.end()) break;

                result.push_back(&nextIt->second);

                current = &nextIt->second;
            }

            return result;
        }

        static void outputDataFileHeaderRow(std::ofstream &file);

        void outputToDataFile(std::ofstream &file, const TilePosition &resourceTile) const;

        static bool parseFromDataFile(
                const std::vector<std::string> &line,
                std::map<TilePosition, std::unordered_map<PositionAndVelocity, GatherPositionObservations>> &map,
                int lineNumber);
    };

    std::ostream &operator<<(std::ostream &os, const GatherPositionObservations &gatherPositionObservations);
}
