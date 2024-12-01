#pragma once

#include "TilePosition.h"
#include "PositionAndVelocity.h"
#include "OrderProcessTimer.h"
#include "Resource.h"
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
        std::unordered_map<int16_t, uint32_t> arrivalDelayAndOccurrences;
        uint32_t collisions = 0;
        uint32_t nonCollisions = 0;

        void add(int16_t arrivalDelay)
        {
            arrivalDelayAndOccurrences[arrivalDelay]++;
        }

        [[nodiscard]] bool empty() const
        {
            return arrivalDelayAndOccurrences.empty();
        }

        [[nodiscard]] int mostCommonArrivalDelay() const;

        [[nodiscard]] double expectedMiningDelay(int commandFrame) const;

        template <typename S>
        void serialize(S& s)
        {
            s.ext(arrivalDelayAndOccurrences, bitsery::ext::StdMap{ INT_MAX }, [](S& s, int16_t& key, uint32_t& value) {
                s.value2b(key);
                s.value4b(value);
            });
            s.value4b(collisions);
            s.value4b(nonCollisions);
        }
    };

    struct SecondResendGatherPositionObservations
    {
        PositionAndVelocity pos;
        uint16_t deltaToFirstResend = 0;
        std::unordered_map<PositionAndVelocity, uint32_t> nextPositionAndOccurrences;
        GatherResendArrivalObservations arrivalObservations;

        template <typename S>
        void serialize(S& s)
        {
            s.object(pos);
            s.value2b(deltaToFirstResend);
            s.ext(nextPositionAndOccurrences, bitsery::ext::StdMap{ INT_MAX }, [](S& s, PositionAndVelocity& key, uint32_t& value) {
                s.object(key);
                s.value4b(value);
            });
            s.object(arrivalObservations);
        }
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
        std::unordered_map<int16_t, uint32_t> deltaToBenchmarkAndOccurrences;

        // All next positions seen from this position, with their count of observations
        // May be empty for the last position we consider in a path
        std::unordered_map<PositionAndVelocity, uint32_t> nextPositionAndOccurrences;

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
                std::unordered_map<int16_t, uint32_t> &&deltaToBenchmarkAndOccurrences,
                std::unordered_map<PositionAndVelocity, uint32_t> &&nextPositionAndOccurrences,
                GatherResendArrivalObservations &&noSecondResendArrivalObservations,
                std::unordered_map<PositionAndVelocity, SecondResendGatherPositionObservations> &&secondResendObservations,
                uint32_t noResendCollisions = 0,
                uint32_t noResendNonCollisions = 0,
                ResendChangesPath resendChangesPath = ResendChangesPath::Unknown)
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

        template <typename S>
        void serialize(S& s) {
            s.value4b(pathHash);
            s.object(pos);
            s.ext(deltaToBenchmarkAndOccurrences, bitsery::ext::StdMap{ INT_MAX }, [](S& s, int16_t& key, uint32_t& value) {
                s.value2b(key);
                s.value4b(value);
            });
            s.ext(nextPositionAndOccurrences, bitsery::ext::StdMap{ INT_MAX }, [](S& s, PositionAndVelocity& key, uint32_t& value) {
                s.object(key);
                s.value4b(value);
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
