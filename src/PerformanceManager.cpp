#include "PerformanceManager.h"

#include "Common.h"

#include <chrono>

#if IS_OPENBW
#define FRAME_LIMIT 15000
#else
// The actual limit for AIIDE / COG is 55ms, but we don't want to get too close to this as the resolution of the BWAPI / TM timer is poor
#define FRAME_LIMIT 35000
#endif

/*
 * This component is responsible for trying to keep us within our frame time limit when running in a time-limited environment.
 *
 * The overall bot frame processing consists of:
 * 1. Updating all kinds of data structures based on what has happened in the game since the previous frame
 *    Examples: unit tracking, grids, building placement updates, assigning units to plays / squads / clusters
 * 2. Running the combat logic (targeting, combat sim, execution of combat decisions)
 * 3. Running the producer and issuing related orders (producing from existing buildings or building new buildings)
 * 4. Running the worker mining optimization
 * 5. Spending any additional available frame time on compute that might otherwise hit us later, e.g. navigation grid updates
 *
 * #1 usually takes approximately 30-35% of the total frame time, but there is significant variance because of grid and navigation grid
 * updates being applied on "random" frames, so its P99 is 70% of total frame time. There are no activities here that are easily skippable,
 * so we always run everything and adjust later activities if we see this step used a lot of time.
 *
 * #2 scales with the number of own and enemy army units, which causes the targeting and especially combat sim to take longer to run as the
 * size of the armies grow. We track the amount of time a combat sim has taken and abort it early if we don't have the budget for a longer
 * sim. Targeting and combat commands are never skipped, since that may cause issues with stuck units.
 *
 * #3 has a fairly consistent "base load" that on its own will never cause problems, but can also be skipped for a few frames without
 * incurring any major problems, so we do this if we see the frame time is high.
 *
 * #4 is extremely variable depending on how many workers are mining and how many of them need to run the path solver on a given frame.
 * Return path optimizations are much less intensive, since we only support one command resend, so they are always allowed to run. Gather
 * path optimization of a base is skipped if we are under time pressure and any of its units needs to run the gather path solver.
 *
 * #5 is supported by allowing actions to be run until we are at half of our frame time budget to try to avoid ever going over.
 */

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
        if (allFrameTimes.empty()) return;

        std::vector<double> allPercentSpentInUpdateStep;
        for (size_t i = 0; i < allFrameTimes.size(); i++)
        {
            if (allFrameTimes[i] > 0)
            {
                allPercentSpentInUpdateStep.push_back((double)allUpdateStepTimes[i] / (double)allFrameTimes[i]);
            }
            else
            {
                allPercentSpentInUpdateStep.push_back(0.0);
            }
        }

        std::sort(allFrameTimes.begin(), allFrameTimes.end());
        std::sort(allUpdateStepTimes.begin(), allUpdateStepTimes.end());
        std::sort(allPercentSpentInUpdateStep.begin(), allPercentSpentInUpdateStep.end());

        Log::Get() << std::fixed << std::setprecision(2)
                   << "Frame total"
                   << ": avg=" << std::left << std::setw(8)
                   << (double)std::reduce(allFrameTimes.begin(), allFrameTimes.end()) / (double)allFrameTimes.size()
                   << "p99.9=" << std::left << std::setw(6) << allFrameTimes[(allFrameTimes.size() * 999) / 1000]
                   << "p99=" << std::left << std::setw(6) << allFrameTimes[(allFrameTimes.size() * 99) / 100]
                   << "p95=" << std::left << std::setw(6) << allFrameTimes[(allFrameTimes.size() * 95) / 100]
                   << "p90=" << std::left << std::setw(6) << allFrameTimes[(allFrameTimes.size() * 90) / 100]
                   << "p50=" << std::left << std::setw(6) << allFrameTimes[(allFrameTimes.size() * 50) / 100];
        Log::Get() << std::fixed << std::setprecision(2)
                   << "Update step"
                   << ": avg=" << std::left << std::setw(8)
                   << (double)std::reduce(allUpdateStepTimes.begin(), allUpdateStepTimes.end()) / (double)allUpdateStepTimes.size()
                   << "p99.9=" << std::left << std::setw(6) << allUpdateStepTimes[(allUpdateStepTimes.size() * 999) / 1000]
                   << "p99=" << std::left << std::setw(6) << allUpdateStepTimes[(allUpdateStepTimes.size() * 99) / 100]
                   << "p95=" << std::left << std::setw(6) << allUpdateStepTimes[(allUpdateStepTimes.size() * 95) / 100]
                   << "p90=" << std::left << std::setw(6) << allUpdateStepTimes[(allUpdateStepTimes.size() * 90) / 100]
                   << "p50=" << std::left << std::setw(6) << allUpdateStepTimes[(allUpdateStepTimes.size() * 50) / 100];
        Log::Get() << std::fixed << std::setprecision(2)
                   << "% Upd. step"
                   << ": avg=" << std::left << std::setw(8)
                   << std::reduce(allPercentSpentInUpdateStep.begin(), allPercentSpentInUpdateStep.end())
                            / (double)allPercentSpentInUpdateStep.size()
                   << "p99.9=" << std::left << std::setw(6) << allPercentSpentInUpdateStep[(allPercentSpentInUpdateStep.size() * 999) / 1000]
                   << "p99=" << std::left << std::setw(6) << allPercentSpentInUpdateStep[(allPercentSpentInUpdateStep.size() * 99) / 100]
                   << "p95=" << std::left << std::setw(6) << allPercentSpentInUpdateStep[(allPercentSpentInUpdateStep.size() * 95) / 100]
                   << "p90=" << std::left << std::setw(6) << allPercentSpentInUpdateStep[(allPercentSpentInUpdateStep.size() * 90) / 100]
                   << "p50=" << std::left << std::setw(6) << allPercentSpentInUpdateStep[(allPercentSpentInUpdateStep.size() * 50) / 100];
    }

    unsigned int combatSimTimeLimitMicroseconds()
    {
#if FRAME_TIME_LIMIT
        auto time = microsecondsFromFrameStart();
        if (time > FRAME_LIMIT / 2)
        {
            return 1000;
        }
        if (time > FRAME_LIMIT / 3)
        {
            return 2000;
        }
        return 5000;
#else
        return 10000;
#endif
    }

    bool shouldSkipProducer()
    {
#if FRAME_TIME_LIMIT
        return microsecondsFromFrameStart() > (FRAME_LIMIT * 2 / 3);
#else
        return false;
#endif
    }

    bool shouldAvoidMiningOptimizationGatherSolver()
    {
#if FRAME_TIME_LIMIT
        return microsecondsFromFrameStart() > (FRAME_LIMIT * 3 / 4);
#else
        return false;
#endif
    }

    bool canPerformOptionalCalculations()
    {
#if FRAME_TIME_LIMIT
        return microsecondsFromFrameStart() < (FRAME_LIMIT * 2 / 3);
#else
        // Technically we can, but there's no reason to do it since lazy evaluation isn't penalized
        return false;
#endif
    }
}
