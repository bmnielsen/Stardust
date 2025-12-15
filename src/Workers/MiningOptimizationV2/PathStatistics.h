#pragma once

#include "DataModel/PositionAndVelocity.h"

namespace MiningOptimization
{
    struct PathStatistics
    {
        // The count of total gathers or returns
        unsigned int count;

        // The subset of the above that had any path data
        unsigned int withPath;

        // The subset of the above that was able to follow the path data to completion
        unsigned int withPathFollowedToCompletion;

        // The subset of the paths followed to completion where the actual arrival frame matched one of the expected values
        unsigned int withExpectedArrivalFrame;

        // The subset of the paths followed to completion where the actual action frame matched one of the expected values
        unsigned int withExpectedActionFrame;

#if IS_OPENBW
        // Set of start positions where we had no path data at all
        std::set<BWAPI::ExactPosition> startPositionsMissingPath;

        // Set of start positions where we lost the path
        std::set<BWAPI::ExactPosition> startPositionsThatLostPath;
#endif

        void reset()
        {
            count = 0;
            withPath = 0;
            withPathFollowedToCompletion = 0;
            withExpectedArrivalFrame = 0;
            withExpectedActionFrame = 0;
#if IS_OPENBW
            startPositionsMissingPath.clear();
            startPositionsThatLostPath.clear();
#endif
        }
    };
}
