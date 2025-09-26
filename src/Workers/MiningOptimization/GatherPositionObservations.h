#pragma once

#include "TilePosition.h"
#include "PositionAndVelocity.h"
#include "OccurrencesAndCollisions.h"
#include "OrderProcessTimer.h"
#include "Resource.h"
#include <map>

namespace WorkerMiningOptimization
{
    struct GatherResendArrivalObservations
    {
        std::unordered_map<int8_t, uint32_t> arrivalDelayAndOccurrences;
        std::unordered_map<int8_t, uint8_t> arrivalDelayAndOccurrenceRate;

        uint32_t collisions = 0;
        uint32_t nonCollisions = 0;
        uint8_t collisionRate = 0;

        bool addArrival(int arrivalDelay);

        void addCollision(bool collision)
        {
            if (collisions + nonCollisions == UINT32_MAX) return;
            (collision ? collisions : nonCollisions)++;
            collisionRate = computeCollisionRate(collisions, nonCollisions);
        }

        [[nodiscard]] bool empty() const
        {
            return arrivalDelayAndOccurrenceRate.empty();
        }

        [[nodiscard]] int8_t mostCommonArrivalDelay() const;

        [[nodiscard]] double expectedMiningDelay(int commandFrame) const;

        static double arrivalDelayToMiningDelay(int arrivalDelay, int commandFrame);
    };

    struct SecondResendGatherPositionObservations
    {
        PositionAndVelocity pos;
        uint32_t occurrences = 1;
        uint8_t occurrenceRate = 0;
        std::vector<SecondResendGatherPositionObservations> nextPositions;
        GatherResendArrivalObservations arrivalObservations;
    };

    // This is the structure we use to track observed positions and our track record using them
    struct GatherPositionObservations
    {
    public:
        // The position
        PositionAndVelocity pos;

        // How often this position has occurred in its path
        // For root nodes, how often it has been observed
        uint32_t occurrences = 0;

        // The occurence rate of this position compared to its siblings as a fraction of 255
        uint8_t occurrenceRate = 0;

        // All next positions seen from this position
        // Will be empty on leaf nodes
        std::vector<GatherPositionObservations> nextPositions;

        // The offset between this resend position and the apparent optimal position if no resend had been issued
        // For positions with unstable following paths this can differ, so we store all observed values with their occurrences
        // May be empty if we haven't observed a no-resend path with this position yet
        std::unordered_map<int8_t, uint32_t> deltaToBenchmarkAndOccurrences;

        // Same as above, but as rates (fraction of 255)
        std::unordered_map<int8_t, uint8_t> deltaToBenchmarkAndOccurrenceRate;

        // Observations for when we send a resend here without a second resend
        GatherResendArrivalObservations noSecondResendArrivalObservations;

        // Start of the path after resending here, where we track the path and behaviour of second resends
        // Empty if a resend never changes the path
        std::vector<SecondResendGatherPositionObservations> secondResendPositions;

        // How many times the worker collides with the patch after mining when no resend is sent along this path
        uint32_t noResendCollisions = 0;

        // How many times the worker does not collide with the patch after mining when no resend is sent along this path
        uint32_t noResendNonCollisions = 0;

        // The collision rate as a fraction of 255
        uint8_t noResendCollisionRate = 0;

        GatherPositionObservations() = default;

        GatherPositionObservations(PositionAndVelocity pos) : pos(pos) {}

        GatherPositionObservations(PositionAndVelocity pos, int deltaToBenchmark) : pos(pos)
        {
            addDeltaToBenchmark(deltaToBenchmark);
        }

        [[nodiscard]] double averageDeltaToBenchmark() const;

        [[nodiscard]] int probableDeltaToBenchmark() const;

        [[nodiscard]] int largestDeltaToBenchmark() const;

        [[nodiscard]] bool usableForPathPlanning() const;

        void addDeltaToBenchmark(int delta)
        {
            if (delta > INT8_MAX || delta < INT8_MIN)
            {
                Log::Get() << "ERROR: deltaToBenchmark " << delta << " outside normal bounds";
                return;
            }

            if (atOccurrenceCap(deltaToBenchmarkAndOccurrences)) return;
            deltaToBenchmarkAndOccurrences[(int8_t)delta]++;
            deltaToBenchmarkAndOccurrenceRate = computeOccurrenceRateMap(deltaToBenchmarkAndOccurrences);
        }

        void addNoResendCollision(bool collision)
        {
            if (noResendCollisions + noResendNonCollisions == UINT32_MAX) return;
            (collision ? noResendCollisions : noResendNonCollisions)++;
            noResendCollisionRate = computeCollisionRate(noResendCollisions, noResendNonCollisions);
        }

//        template<typename K>
//        requires std::is_same_v<const K, const std::unordered_map<PositionAndVelocity, GatherPositionObservations>>
//        [[nodiscard]] auto followingPositionsIfStable(K &positionsData) const
//        {
//            using elem = std::remove_reference_t<decltype((positionsData.begin()->second))>;
//            std::vector<elem *> result;
//            const GatherPositionObservations *current = this;
//            std::unordered_set<PositionAndVelocity> visited;
//            while (!current->nextPositionAndOccurrences.empty())
//            {
//                if (visited.contains(current->pos) || current->nextPositionAndOccurrences.size() > 1)
//                {
//                    return std::vector<elem *>{};
//                }
//
//                visited.insert(current->pos);
//
//                auto nextIt = positionsData.find(current->nextPositionAndOccurrences.begin()->first);
//                if (nextIt == positionsData.end()) break;
//
//                result.push_back(&nextIt->second);
//
//                current = &nextIt->second;
//            }
//
//            return result;
//        }
    };

    // Struct we use to track the position history as pointers to observations
    struct GatherPositionObservationPtr
    {
        GatherPositionObservations *pos;
        SecondResendGatherPositionObservations *secondResendPos;

        explicit GatherPositionObservationPtr(GatherPositionObservations *pos)
                : pos(pos)
                , secondResendPos(nullptr)
        {}

        explicit GatherPositionObservationPtr(SecondResendGatherPositionObservations *secondResendPos)
                : pos(nullptr)
                , secondResendPos(secondResendPos)
        {}

        bool operator==(const GatherPositionObservationPtr &other) const
        {
            return (pos && other.pos && pos->pos == other.pos->pos) ||
                   (secondResendPos && other.secondResendPos && secondResendPos->pos == other.secondResendPos->pos);
        }

        [[nodiscard]] PositionAndVelocity &position() const
        {
            return pos ? pos->pos : secondResendPos->pos;
        }

        [[nodiscard]] std::vector<SecondResendGatherPositionObservations> &nextSecondResendPositions() const
        {
            return (pos ? pos->secondResendPositions : secondResendPos->nextPositions);
        }

        [[nodiscard]] GatherResendArrivalObservations &resendArrivalObservations() const
        {
            return (pos ? pos->noSecondResendArrivalObservations : secondResendPos->arrivalObservations);
        }

        [[nodiscard]] std::unique_ptr<GatherPositionObservationPtr> nextPositionIfExists(
                const PositionAndVelocity &nextPos, const std::shared_ptr<const PositionAndVelocity> &resendPos) const
        {
            if (pos)
            {
                if (resendPos && pos->pos == *resendPos)
                {
                    return findInNextPositionsVector(pos->secondResendPositions, nextPos);
                }
                return findInNextPositionsVector(pos->nextPositions, nextPos);
            }
            return findInNextPositionsVector(secondResendPos->nextPositions, nextPos);
        }

        friend std::ostream &operator<<(std::ostream &os, const GatherPositionObservationPtr &ptr);

    private:
        template<typename T>
        [[nodiscard]] std::unique_ptr<GatherPositionObservationPtr> findInNextPositionsVector(std::vector<T> &nextPositions,
                                                                                              const PositionAndVelocity &nextPos) const
        {
            for (auto &candidate : nextPositions)
            {
                if (candidate.pos == nextPos)
                {
                    return std::make_unique<GatherPositionObservationPtr>(&candidate);
                }
            }
            return nullptr;
        }
    };

    std::ostream &operator<<(std::ostream &os, const GatherPositionObservations &gatherPositionObservations);
}
