﻿#include "Timer.h"
#include "Common.h"

#include <chrono>

#define DEBUG_LOG_EACH_CHECKPOINT false

namespace Timer
{
    namespace
    {
#ifdef DEBUG
        const int LOG_CUTOFF = 10000;
        const int DEBUG_CUTOFF = 10000;
#elif defined(INSTRUMENTATION_ENABLED_VERBOSE)
        const int LOG_CUTOFF = 1000;
        const int DEBUG_CUTOFF = 1000;
#elif defined(INSTRUMENTATION_ENABLED)
        const int LOG_CUTOFF = 100;
        const int DEBUG_CUTOFF = 100;
#else
        const int LOG_CUTOFF = 55;
        const int DEBUG_CUTOFF = 45;
#endif

        std::string overallLabel;
        std::chrono::steady_clock::time_point startPoint;
        std::chrono::steady_clock::time_point lastCheckpoint;
        std::vector<std::pair<std::string, long long>> checkpoints;

        bool sortCheckpoints(std::pair<std::string, long long> &first, std::pair<std::string, long long> &second)
        {
            return first.second > second.second;
        }
    }

    void start(const std::string &label)
    {
        overallLabel = label;
        checkpoints.clear();
        startPoint = lastCheckpoint = std::chrono::high_resolution_clock::now();
    }

    void checkpoint(const std::string &label)
    {
#if DEBUG_LOG_EACH_CHECKPOINT
        Log::Debug() << label;
#endif
        auto now = std::chrono::high_resolution_clock::now();
        checkpoints.emplace_back(label, std::chrono::duration_cast<std::chrono::microseconds>(now - lastCheckpoint).count());
        lastCheckpoint = now;
    }

    void stop(bool forceOutput)
    {
        long long overall = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - startPoint).count();

        if (forceOutput || overall > DEBUG_CUTOFF)
        {
            std::ostringstream msg;
            msg << overallLabel << " took " << overall << "ms";

            std::sort(checkpoints.begin(), checkpoints.end(), sortCheckpoints);

            bool first = true;
            for (auto &checkpoint : checkpoints)
            {
                if (checkpoint.second < (DEBUG_CUTOFF / 5)) continue;

                if (first)
                {
                    msg << ", longest: ";
                    first = false;
                }
                else
                    msg << ", ";

                msg << checkpoint.first << ": " << checkpoint.second << "us";
            }

            Log::Debug() << msg.str();

            if (forceOutput || overall > LOG_CUTOFF)
                Log::Get() << msg.str();
        }
    }
}
