#include "PerformanceManager.h"

#include "Common.h"

#include <chrono>

#define FRAME_LIMIT 15000

namespace PerformanceManager
{
    namespace
    {
        // Vectors to track overall statistics
        std::vector<long long> allFrameTimes;
        std::vector<long long> allUpdateStepTimes;

        // Set at the start of every frame
        std::chrono::steady_clock::time_point frameStartTime;

        long long microsecondsFromFrameStart()
        {
            return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - frameStartTime).count();
        }
    }

    void initialize()
    {
        allFrameTimes.clear();
        allFrameTimes.reserve(30000);
        allUpdateStepTimes.clear();
        allUpdateStepTimes.reserve(30000);
    }

    void frameStart()
    {
        frameStartTime = std::chrono::high_resolution_clock::now();
    }

    bool frameEnd()
    {
        if (currentFrame < 1) return false;

        auto time = microsecondsFromFrameStart();
        allFrameTimes.push_back(time);

        if (time > FRAME_LIMIT)
        {
            Log::Get() << "Frame time exceeded limit: " << time << " vs. " << FRAME_LIMIT
                       << " (spent in update step: " << allUpdateStepTimes.back() << ")";
            return true;
        }

        return false;
    }

    void frameFinishedUpdateStep()
    {
        if (currentFrame < 1) return;

        allUpdateStepTimes.push_back(microsecondsFromFrameStart());
    }

    void gameEnd()
    {
        if (allFrameTimes.size() == 0) return;

        long long averageFrameTimeAggregator = 0;
        long long maxFrameTime = 0;

        long long averageUpdateStepTimeAggregator = 0;
        long long maxUpdateStepTime = 0;

        size_t percentageSpentInUpdateAggregatorCount = 0;
        double percentageSpentInUpdateAggregator = 0.0;
        double maxPercentageSpentInUpdate = 0.0;

        for (size_t i = 0; i < allFrameTimes.size(); i++)
        {
            averageFrameTimeAggregator += allFrameTimes[i];
            maxFrameTime = std::max(maxFrameTime, allFrameTimes[i]);

            averageUpdateStepTimeAggregator += allUpdateStepTimes[i];
            maxUpdateStepTime = std::max(maxUpdateStepTime, allUpdateStepTimes[i]);

            if (allFrameTimes[i] > 0)
            {
                double percentageSpentInUpdate = (double)allUpdateStepTimes[i] / (double)allFrameTimes[i];
                percentageSpentInUpdateAggregator += percentageSpentInUpdate;
                percentageSpentInUpdateAggregatorCount++;
                maxPercentageSpentInUpdate = std::max(maxPercentageSpentInUpdate, percentageSpentInUpdate);
            }
        }

        double averageFrameTime = (double)averageFrameTimeAggregator / (double)allFrameTimes.size();
        double averageUpdateStepTime = (double)averageUpdateStepTimeAggregator / (double)allUpdateStepTimes.size();
        double averagePercentageSpentInUpdate = 0.0;
        if (percentageSpentInUpdateAggregatorCount > 0)
        {
            averagePercentageSpentInUpdate = 100.0 * percentageSpentInUpdateAggregator / (double)percentageSpentInUpdateAggregatorCount;
        }
        maxPercentageSpentInUpdate *= 100.0;

        Log::Get() << std::fixed << std::setprecision(2)
                   << "Average frame time: " << averageFrameTime << " us (max " << maxFrameTime << " us)";
        Log::Get() << std::fixed << std::setprecision(2)
                   << "Average update step time: " << averageUpdateStepTime << " us (max " << maxUpdateStepTime << " us)";
        Log::Get() << std::fixed << std::setprecision(2)
                   << "Average percentage spend in update: " << averagePercentageSpentInUpdate << "% (max " << maxPercentageSpentInUpdate << "%)";
    }
}
