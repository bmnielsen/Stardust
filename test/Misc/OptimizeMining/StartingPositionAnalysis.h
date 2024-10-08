#pragma once

#include <BWAPI.h>
#include <ranges>

#include "MiningPath.h"

#include "Log.h"
#include "CherryVis.h"

struct PathAnalysis
{
    bool stable = false;
    bool twoCycleStable = false;
    bool lastTwoStable = false;
    // 1 = extra cycle of waiting time at start; 2 = single extra frame; 3 = same length but different path; 0 = other
    int unstableType = 0;
    size_t shortestLength = INT_MAX;
    size_t shortestIndex = 0;
    size_t longestLength = 0;
    size_t longestIndex = 0;

    std::vector<Path> paths;

    void analyze(std::ranges::input_range auto&& range)
    {
        paths.assign(range.begin(), range.end());

        // We look at the last three paths where we assume the path should have stabilized

        for (int i = 2; i < 5; i++)
        {
            if (paths[i].size() < shortestLength)
            {
                shortestLength = paths[i].size();
                shortestIndex = i;
            }
            if (paths[i].size() > longestLength)
            {
                longestLength = paths[i].size();
                longestIndex = i;
            }
        }

        stable = (paths[2].hash() == paths[3].hash() && paths[2].hash() == paths[4].hash());

        if (!stable)
        {
            CherryVis::log() << "Path unstable: shortest=" << shortestLength << "; longest=" << longestLength;
            if (longestLength > shortestLength)
            {
                if (longestLength - shortestLength == 1)
                {
                    unstableType = 2;
                    CherryVis::log() << "One frame difference";
                }
                else if (longestLength - shortestLength == 11 && (*paths[longestIndex].begin()) == (*(paths[longestIndex].begin() + 8)))
                {
                    unstableType = 1;
                }
                CherryVis::log() << "Shortest path (" << shortestIndex << "): \n" << paths[shortestIndex];
                CherryVis::log() << "Longest path (" << longestIndex << "): \n" << paths[longestIndex];
            }
            else
            {
                unstableType = 3;
                CherryVis::log() << "Same length";
            }

            if (paths[1].hash() == paths[3].hash() && paths[2].hash() == paths[4].hash())
            {
                twoCycleStable = true;
            }
            else if (paths[3].hash() == paths[4].hash())
            {
                lastTwoStable = true;
            }
        }
    }
};

struct StartingPositionAnalysis
{
    BWAPI::TilePosition patchTile;
    BWAPI::Position startingPosition;
    std::vector<MiningPath> miningPaths;

    PathAnalysis gatherPathAnalysis;
    PathAnalysis returnPathAnalysis;

    void analyze()
    {
        if (miningPaths.size() != 5)
        {
            Log::Get() << "ERROR: Start position " << startingPosition << " only has " << miningPaths.size() << " mining paths";
            return;
        }

        gatherPathAnalysis.analyze(miningPaths | std::views::transform([](const MiningPath &miningPath){ return miningPath.gatherPath; }));
        returnPathAnalysis.analyze(miningPaths | std::views::transform([](const MiningPath &miningPath){ return miningPath.returnPath; }));
    }
};
