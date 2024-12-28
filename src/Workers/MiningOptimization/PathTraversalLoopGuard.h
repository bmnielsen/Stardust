#pragma once

#include "PositionAndVelocity.h"

#define RECURSION_LIMIT 100

namespace WorkerMiningOptimization
{
    /*
     * Guards against loops when we are traversing path data in the mining optimizers.
     * In the release build, we use a set of positions to track this very accurately.
     * In non-release builds, the set operations are costly because of sanitizers, so we replace it with a simple depth counter.
     */
    class PathTraversalLoopGuard
    {
    public:
        // Pushes a next position onto the traversal stack, returning true if it has already been seen.
        bool push(const PositionAndVelocity &pos)
        {
#if INSTRUMENTATION_ENABLED
            depth++;
            return depth == RECURSION_LIMIT;
#else
            auto result = visited.insert(here);
            return !result.second || visited.size() == RECURSION_LIMIT;
#endif
        }

        // Pops the latest position from the stack.
        void pop(const PositionAndVelocity &pos)
        {
#if INSTRUMENTATION_ENABLED
            depth--;
#else
            visited.erase(pos);
#endif
        }
    private:
#if INSTRUMENTATION_ENABLED
        int depth = 0;
#else
        std::unordered_set<PositionAndVelocity> visited;
#endif
    };
}
