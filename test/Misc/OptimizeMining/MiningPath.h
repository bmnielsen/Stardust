#pragma once

#include <BWAPI.h>

#include "CsvTools.h"
#include "MiningOptimization/PositionAndVelocity.h"

class Path : public std::vector<PositionAndVelocity>
{
public:
    // Adapted from https://stackoverflow.com/questions/20511347/a-good-hash-function-for-a-vector/72073933#72073933
    uint32_t hash()
    {
        if (hasCachedHash) return cachedHash;

        uint32_t seed = size();
        auto addNumber = [&seed](int x)
        {
            x = ((x >> 16) ^ x) * 0x45d9f3b;
            x = ((x >> 16) ^ x) * 0x45d9f3b;
            x = (x >> 16) ^ x;
            seed ^= x + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        };
        for (const auto &pos : *this)
        {
            addNumber(pos.x);
            addNumber(pos.y);
            addNumber(pos.dx);
            addNumber(pos.dy);
            addNumber(pos.heading);
        }

        cachedHash = seed;
        hasCachedHash = true;
        return seed;
    }

    friend std::ostream &operator<<(std::ostream &os, const Path &path)
    {
        CsvTools::outputList(os, path);
        return os;
    }

private:
    bool hasCachedHash = false;
    uint32_t cachedHash = 0;
};

struct MiningPath
{
    explicit MiningPath(const BWAPI::Unit &probe)
    {
        returnPath.emplace_back(probe);
    }

    Path gatherPath;
    Path returnPath;
};

std::ostream &operator<<(std::ostream &os, const std::vector<BWAPI::Position> &vec);
