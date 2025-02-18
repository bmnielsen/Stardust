#pragma once

#include <map>

namespace MapUtil
{
    template<class T>
    bool atOccurrenceCap(const std::unordered_map<T, uint16_t> &occurrenceMap)
    {
        uint16_t total = 0;
        for (const auto &[_, occurrences] : occurrenceMap)
        {
            total += occurrences;
        }
        return total == UINT16_MAX;
    }

    template<class T>
    bool atOccurrenceCap(const std::vector<T> &objectsWithOccurrence)
    {
        uint16_t total = 0;
        for (const auto &item : objectsWithOccurrence)
        {
            total += item.occurrences;
        }
        return total == UINT16_MAX;
    }
}
