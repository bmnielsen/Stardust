#pragma once

#include "Common.h"
#include "PositionAndVelocity.h"

namespace WorkerMiningOptimization
{
    template<class T>
    [[nodiscard]] bool atOccurrenceCap(const std::unordered_map<T, uint32_t> &occurrenceMap)
    {
        uint32_t total = 0;
        for (const auto &[_, occurrences] : occurrenceMap)
        {
            total += occurrences;
        }
        return total == UINT32_MAX;
    }

    template<class T>
    [[nodiscard]] std::pair<T*, bool> findNextPositionCheckingOccurrences(const PositionAndVelocity &pos, std::vector<T> &nextPositions)
    {
        uint32_t total = 0;
        T* match = nullptr;
        for (auto &nextPosition : nextPositions)
        {
            total += nextPosition.occurrences;
            if (nextPosition.pos == pos) match = &nextPosition;
        }
        return std::make_pair(match, total == UINT32_MAX);
    }

    template<class T>
    [[nodiscard]] std::unordered_map<T, uint8_t> computeOccurrenceRateMap(const std::unordered_map<T, uint32_t> &occurrenceMap)
    {
        uint32_t total = 0;
        for (const auto &[_, occurrences] : occurrenceMap)
        {
            total += occurrences;
        }
        std::unordered_map<T, uint8_t> result;
        for (const auto &[key, occurrences] : occurrenceMap)
        {
            result[key] = (uint8_t)(std::round(255.0 * ((double)occurrences/(double)(total))));
        }
        return result;
    }

    template<class T>
    void updateNextOccurenceRates(std::vector<T> &nextPositions)
    {
        uint32_t total = 0;
        for (const auto &nextPosition : nextPositions)
        {
            total += nextPosition.occurrences;
        }
        for (auto &nextPosition : nextPositions)
        {
            nextPosition.occurrenceRate = (uint8_t)(std::round(255.0 * ((double)nextPosition.occurrences/(double)(total))));
        }
    }

    [[nodiscard]] uint8_t computeCollisionRate(uint32_t collisions, uint32_t nonCollisions);
}