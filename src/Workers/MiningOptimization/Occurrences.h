#pragma once

#include "Common.h"
#include "PositionAndVelocity.h"

// Configuration of occurrence counts
typedef uint8_t OCCURRENCE_TYPE;
#define OCCURRENCE_LIMIT UINT8_MAX
#define SERIALIZE_OCCURRENCE(X) s.value1b(X)

// Configuration of collision counts
typedef uint16_t COLLISION_TYPE;
#define COLLISION_LIMIT UINT16_MAX
#define SERIALIZE_COLLISION(X) s.value2b(X)

namespace WorkerMiningOptimization
{
    template<class T>
    bool atOccurrenceCap(const std::unordered_map<T, OCCURRENCE_TYPE> &occurrenceMap)
    {
        OCCURRENCE_TYPE total = 0;
        for (const auto &[_, occurrences] : occurrenceMap)
        {
            total += occurrences;
        }
        return total == OCCURRENCE_LIMIT;
    }

    template<class T>
    std::pair<T*, bool> findNextPositionCheckingOccurrences(const PositionAndVelocity &pos, std::vector<T> &nextPositions)
    {
        OCCURRENCE_TYPE total = 0;
        T* match = nullptr;
        for (auto &nextPosition : nextPositions)
        {
            total += nextPosition.occurrences;
            if (nextPosition.pos == pos) match = &nextPosition;
        }
        return std::make_pair(match, total == OCCURRENCE_LIMIT);
    }
}