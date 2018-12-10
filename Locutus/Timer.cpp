#include "Timer.h"
#include "Common.h"

#include <chrono>

namespace Timer
{
    namespace 
    {
        const int LOG_CUTOFF = 35;
        const int DEBUG_CUTOFF = 10;

        std::string                                    overallLabel;
        std::chrono::steady_clock::time_point          startPoint;
        std::chrono::steady_clock::time_point          lastCheckpoint;
        std::vector<std::pair<std::string, long long>> checkpoints;

        bool sortCheckpoints(std::pair<std::string, long long> & first, std::pair<std::string, long long> & second)
        { 
            return first.second > second.second;
        }
    }

    void start(std::string label)
    {
        overallLabel = label;
        checkpoints.clear();
        startPoint = lastCheckpoint = std::chrono::high_resolution_clock::now();
    }

    void checkpoint(std::string label)
    {
        auto now = std::chrono::high_resolution_clock::now();
        checkpoints.push_back(std::make_pair(label, std::chrono::duration_cast<std::chrono::microseconds>(now - lastCheckpoint).count()));
        lastCheckpoint = now;
    }

    void stop()
    {
        long long overall = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - startPoint).count();

        if (overall > DEBUG_CUTOFF)
        {
            std::ostringstream msg;
            msg << overallLabel << " took " << overall << "ms";

            std::sort(checkpoints.begin(), checkpoints.end(), sortCheckpoints);

            bool first = true;
            for (auto & checkpoint : checkpoints)
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

            if (overall > LOG_CUTOFF)
                Log::Get() << msg.str();
        }
    }
}
