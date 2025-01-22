#pragma once

#include "TilePosition.h"
#include "PositionAndVelocity.h"
#include "OrderProcessTimer.h"
#include "Resource.h"
#include "MapUtil.h"
#include <bitsery/ext/std_map.h>
#include <map>

namespace WorkerMiningOptimization
{
    enum class ResendChangesPath : uint8_t
    {
        Unknown,
        Yes,
        No
    };
    struct GatherResendArrivalObservations
    {
        std::unordered_map<int8_t, uint16_t> arrivalDelayAndOccurrences;
        uint16_t collisions = 0;
        uint16_t nonCollisions = 0;

        void addArrival(int8_t arrivalDelay)
        {
            if (MapUtil::atOccurrenceCap(arrivalDelayAndOccurrences)) return;
            arrivalDelayAndOccurrences[arrivalDelay]++;
        }

        void addCollision(bool collision)
        {
            if (collisions + nonCollisions == UINT16_MAX) return;
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
            s.ext(arrivalDelayAndOccurrences, bitsery::ext::StdMap{ INT_MAX }, [](S& s, int8_t& key, uint16_t& value) {
                s.value1b(key);
                s.value2b(value);
            });
            s.value2b(collisions);
            s.value2b(nonCollisions);
        }
    };

    struct SecondResendGatherPositionObservations
    {
        uint8_t deltaToFirstResend = 0;
        std::unordered_map<PositionAndVelocity, uint16_t> nextPositionAndOccurrences;
        GatherResendArrivalObservations arrivalObservations;

        void addNext(const PositionAndVelocity &next)
        {
            if (MapUtil::atOccurrenceCap(nextPositionAndOccurrences)) return;
            nextPositionAndOccurrences[next]++;
        }

        template <typename S>
        void serialize(S& s)
        {
            s.value1b(deltaToFirstResend);
            s.ext(nextPositionAndOccurrences, bitsery::ext::StdMap{ INT_MAX }, [](S& s, PositionAndVelocity& key, uint16_t& value) {
                s.object(key);
                s.value2b(value);
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

        // The offset between this resend position and the apparent optimal position if no resend had been issued
        // For positions with unstable following paths this can differ, so we store all observed values with their occurrences
        // May be empty if we haven't observed a no-resend path with this position yet
        std::unordered_map<int8_t, uint16_t> deltaToBenchmarkAndOccurrences;

        // All next positions seen from this position, with their count of observations
        // May be empty for the last position we consider in a path
        std::unordered_map<PositionAndVelocity, uint16_t> nextPositionAndOccurrences;

        // Observations for when we send a resend here without a second resend
        GatherResendArrivalObservations noSecondResendArrivalObservations;

        // Observations for second resends after resending at this position
        std::unordered_map<PositionAndVelocity, SecondResendGatherPositionObservations> secondResendObservations;

        // How many times the worker collides with the patch after mining when no resend is sent along this path
        uint16_t noResendCollisions = 0;

        // How many times the worker does not collide with the patch after mining when no resend is sent along this path
        uint16_t noResendNonCollisions = 0;

        // Whether a resend at this position changes the path
        ResendChangesPath resendChangesPath = ResendChangesPath::Unknown;

        GatherPositionObservations() = default;

        explicit GatherPositionObservations(PositionAndVelocity pos)
                : pos(pos)
        {}

        GatherPositionObservations(PositionAndVelocity pos, int deltaToBenchmark) : pos(pos)
        {
            addDeltaToBenchmark(deltaToBenchmark);
        }

        [[nodiscard]] double averageDeltaToBenchmark() const;

        [[nodiscard]] int probableDeltaToBenchmark() const;

        [[nodiscard]] int largestDeltaToBenchmark() const;

        [[nodiscard]] bool usableForPathPlanning() const;

        bool addArrivalObservation(SecondResendGatherPositionObservations *secondResendPositionData, int arrivalDelta);

        void addDeltaToBenchmark(int delta)
        {
            if (delta > INT8_MAX || delta < INT8_MIN)
            {
                Log::Get() << "ERROR: deltaToBenchmark " << delta << " outside normal bounds";
                return;
            }

            if (MapUtil::atOccurrenceCap(deltaToBenchmarkAndOccurrences)) return;
            deltaToBenchmarkAndOccurrences[(int8_t)delta]++;
        }

        void addNext(const PositionAndVelocity &next)
        {
            if (MapUtil::atOccurrenceCap(nextPositionAndOccurrences)) return;
            nextPositionAndOccurrences[next]++;
        }

        void addNoResendCollision(bool collision)
        {
            if (noResendCollisions + noResendNonCollisions == UINT16_MAX) return;
            (collision ? noResendCollisions : noResendNonCollisions)++;
        }

        [[nodiscard]] const SecondResendGatherPositionObservations *secondResendObservationsFor(const PositionAndVelocity *secondResendPosition) const
        {
            if (!secondResendPosition) return nullptr;

            auto secondResendDataIt = secondResendObservations.find(*secondResendPosition);
            return (secondResendDataIt == secondResendObservations.end()) ? nullptr : &secondResendDataIt->second;
        }

        [[nodiscard]] SecondResendGatherPositionObservations *secondResendObservationsFor(const PositionAndVelocity *secondResendPosition)
        {
            return const_cast<SecondResendGatherPositionObservations *>(
                    static_cast<const GatherPositionObservations &>(*this).secondResendObservationsFor(secondResendPosition));
        }

        template<typename K>
        requires std::is_same_v<const K, const std::unordered_map<PositionAndVelocity, GatherPositionObservations>>
        [[nodiscard]] auto followingPositionsIfStable(K &positionsData) const
        {
            using elem = std::remove_reference_t<decltype((positionsData.begin()->second))>;
            std::vector<elem *> result;
            const GatherPositionObservations *current = this;
            std::unordered_set<PositionAndVelocity> visited;
            while (!current->nextPositionAndOccurrences.empty())
            {
                if (visited.contains(current->pos) || current->nextPositionAndOccurrences.size() > 1)
                {
                    return std::vector<elem *>{};
                }

                visited.insert(current->pos);

                auto nextIt = positionsData.find(current->nextPositionAndOccurrences.begin()->first);
                if (nextIt == positionsData.end()) break;

                result.push_back(&nextIt->second);

                current = &nextIt->second;
            }

            return result;
        }

        template <typename S>
        void serialize(S& s) {
            s.ext(deltaToBenchmarkAndOccurrences, bitsery::ext::StdMap{ INT_MAX }, [](S& s, int8_t& key, uint16_t& value) {
                s.value1b(key);
                s.value2b(value);
            });
            s.ext(nextPositionAndOccurrences, bitsery::ext::StdMap{ INT_MAX }, [](S& s, PositionAndVelocity& key, uint16_t& value) {
                s.object(key);
                s.value2b(value);
            });
            s.object(noSecondResendArrivalObservations);
            s.ext(secondResendObservations,
                  bitsery::ext::StdMap{INT_MAX},
                  [](S &s, PositionAndVelocity &key, SecondResendGatherPositionObservations &value)
                  {
                      s.object(key);
                      s.object(value);
                  });
            s.value2b(noResendCollisions);
            s.value2b(noResendNonCollisions);
            s.value1b(resendChangesPath);
        }
    };

    std::ostream &operator<<(std::ostream &os, const GatherPositionObservations &gatherPositionObservations);
}
