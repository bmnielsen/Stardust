#pragma once

#include "TilePosition.h"
#include "PositionAndVelocity.h"
#include "Occurrences.h"
#include "OrderProcessTimer.h"
#include "Resource.h"
#include <bitsery/ext/std_map.h>
#include <bitsery/traits/vector.h>
#include <map>

namespace WorkerMiningOptimization
{
    struct GatherResendArrivalObservations
    {
        std::unordered_map<int8_t, OCCURRENCE_TYPE> arrivalDelayAndOccurrences;
        COLLISION_TYPE collisions = 0;
        COLLISION_TYPE nonCollisions = 0;

        bool addArrival(int arrivalDelay);

        void addCollision(bool collision)
        {
            if (collisions + nonCollisions == COLLISION_LIMIT) return;
            (collision ? collisions : nonCollisions)++;
        }

        [[nodiscard]] bool empty() const
        {
            return arrivalDelayAndOccurrences.empty();
        }

        [[nodiscard]] int8_t mostCommonArrivalDelay() const;

        [[nodiscard]] double expectedMiningDelay(int commandFrame) const;

        static double arrivalDelayToMiningDelay(int arrivalDelay, int commandFrame);

        template <typename S>
        void serialize(S& s)
        {
            s.ext(arrivalDelayAndOccurrences, bitsery::ext::StdMap{INT_MAX}, [](S& s, int8_t& key, OCCURRENCE_TYPE& value) {
                s.value1b(key);
                SERIALIZE_OCCURRENCE(value);
            });
            SERIALIZE_COLLISION(collisions);
            SERIALIZE_COLLISION(nonCollisions);
        }
    };

    struct SecondResendGatherPositionObservations
    {
        PositionAndVelocity pos;
        OCCURRENCE_TYPE occurrences = 1;
        std::vector<SecondResendGatherPositionObservations> nextPositions;
        GatherResendArrivalObservations arrivalObservations;

        template <typename S>
        void serialize(S& s)
        {
            s.object(pos);
            SERIALIZE_OCCURRENCE(occurrences);
            s.container(nextPositions, INT_MAX, [](auto &s, SecondResendGatherPositionObservations &value) {
                s.object(value);
            });
            s.object(arrivalObservations);
        }
    };

    // This is the structure we use to track observed positions and our track record using them
    struct GatherPositionObservations
    {
    public:
        // The position
        PositionAndVelocity pos;

        // How often this position has occurred in its path
        // For root nodes, how often it has been observed
        OCCURRENCE_TYPE occurrences = 1;

        // All next positions seen from this position
        // Will be empty on leaf nodes
        std::vector<GatherPositionObservations> nextPositions;

        // The offset between this resend position and the apparent optimal position if no resend had been issued
        // For positions with unstable following paths this can differ, so we store all observed values with their occurrences
        // May be empty if we haven't observed a no-resend path with this position yet
        std::unordered_map<int8_t, OCCURRENCE_TYPE> deltaToBenchmarkAndOccurrences;

        // Observations for when we send a resend here without a second resend
        GatherResendArrivalObservations noSecondResendArrivalObservations;

        // Start of the path after resending here, where we track the path and behaviour of second resends
        // Empty if a resend never changes the path
        std::vector<SecondResendGatherPositionObservations> secondResendPositions;

        // How many times the worker collides with the patch after mining when no resend is sent along this path
        COLLISION_TYPE noResendCollisions = 0;

        // How many times the worker does not collide with the patch after mining when no resend is sent along this path
        COLLISION_TYPE noResendNonCollisions = 0;

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
        }

        void addNoResendCollision(bool collision)
        {
            if (noResendCollisions + noResendNonCollisions == COLLISION_LIMIT) return;
            (collision ? noResendCollisions : noResendNonCollisions)++;
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

        template <typename S>
        void serialize(S& s) {
            s.object(pos);
            SERIALIZE_OCCURRENCE(occurrences);
            s.ext(deltaToBenchmarkAndOccurrences, bitsery::ext::StdMap{INT_MAX}, [](S& s, int8_t& key, OCCURRENCE_TYPE& value) {
                s.value1b(key);
                SERIALIZE_OCCURRENCE(value);
            });
            s.container(nextPositions, INT_MAX, [](auto &s, GatherPositionObservations &value) {
                s.object(value);
            });
            s.object(noSecondResendArrivalObservations);
            s.container(secondResendPositions, INT_MAX, [](auto &s, SecondResendGatherPositionObservations &value) {
                s.object(value);
            });
            SERIALIZE_COLLISION(noResendCollisions);
            SERIALIZE_COLLISION(noResendNonCollisions);
        }
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
