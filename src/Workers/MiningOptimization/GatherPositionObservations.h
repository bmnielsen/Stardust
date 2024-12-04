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
        uint32_t collisions = 0;
        uint32_t nonCollisions = 0;
        std::unordered_map<int8_t, uint16_t> arrivalDelayAndOccurrences;

        void addArrival(int8_t arrivalDelay)
        {
            if (MapUtil::atOccurrenceCap(arrivalDelayAndOccurrences)) return;
            arrivalDelayAndOccurrences[arrivalDelay]++;
        }

        [[nodiscard]] bool empty() const
        {
            return arrivalDelayAndOccurrences.empty();
        }

        [[nodiscard]] int8_t mostCommonArrivalDelay() const;

        [[nodiscard]] double expectedMiningDelay(int commandFrame) const;

        template <typename S>
        void serialize(S& s)
        {
            s.ext(arrivalDelayAndOccurrences, bitsery::ext::StdMap{ INT_MAX }, [](S& s, int8_t& key, uint16_t& value) {
                s.value1b(key);
                s.value2b(value);
            });
            s.value4b(collisions);
            s.value4b(nonCollisions);
        }
    };

    struct SecondResendGatherPositionObservations
    {
        PositionAndVelocity pos;
        uint16_t deltaToFirstResend = 0;
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
            s.object(pos);
            s.value2b(deltaToFirstResend);
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
        std::unordered_map<int16_t, uint32_t> deltaToBenchmarkAndOccurrences;

        // All next positions seen from this position, with their count of observations
        // May be empty for the last position we consider in a path
        std::unordered_map<PositionAndVelocity, uint16_t> nextPositionAndOccurrences;

        // Observations for when we send a resend here without a second resend
        GatherResendArrivalObservations noSecondResendArrivalObservations;

        // Observations for second resends after resending at this position
        std::unordered_map<PositionAndVelocity, SecondResendGatherPositionObservations> secondResendObservations;

        // How many times the worker collides with the patch after mining when no resend is sent along this path
        uint32_t noResendCollisions = 0;

        // How many times the worker does not collide with the patch after mining when no resend is sent along this path
        uint32_t noResendNonCollisions = 0;

        // Whether a resend at this position changes the path
        ResendChangesPath resendChangesPath = ResendChangesPath::Unknown;

        GatherPositionObservations(){}

        GatherPositionObservations(PositionAndVelocity pos)
                : pos(pos)
        {}

        GatherPositionObservations(PositionAndVelocity pos, int deltaToBenchmarkAndOccurrences)
                : pos(pos)
                , deltaToBenchmarkAndOccurrences({{deltaToBenchmarkAndOccurrences, 1}})
        {}

        [[nodiscard]] double averageDeltaToBenchmark() const;

        [[nodiscard]] int probableDeltaToBenchmark() const;

        [[nodiscard]] int largestDeltaToBenchmark() const;

        bool addArrivalObservation(SecondResendGatherPositionObservations *secondResendPositionData, int arrivalDelta);

        void addNext(const PositionAndVelocity &next)
        {
            if (MapUtil::atOccurrenceCap(nextPositionAndOccurrences)) return;
            nextPositionAndOccurrences[next]++;
        }

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

        template <typename S>
        void serialize(S& s) {
            s.object(pos);
            s.ext(deltaToBenchmarkAndOccurrences, bitsery::ext::StdMap{ INT_MAX }, [](S& s, int16_t& key, uint32_t& value) {
                s.value2b(key);
                s.value4b(value);
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
            s.value4b(noResendCollisions);
            s.value4b(noResendNonCollisions);
            s.value1b(resendChangesPath);
        }
    };

    std::ostream &operator<<(std::ostream &os, const GatherPositionObservations &gatherPositionObservations);
}
